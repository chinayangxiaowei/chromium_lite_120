// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/metrics/app_discovery_metrics.h"

#include "base/logging.h"
#include "chrome/browser/apps/app_service/app_service_proxy_ash.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/extension_apps_utils.h"
#include "chrome/browser/ash/borealis/borealis_util.h"
#include "chrome/browser/ash/guest_os/guest_os_registry_service.h"
#include "chrome/browser/ash/guest_os/guest_os_registry_service_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "components/metrics/structured/structured_events.h"
#include "components/services/app_service/public/cpp/instance.h"
#include "components/services/app_service/public/cpp/package_id.h"
#include "components/sync/base/model_type.h"
#include "components/sync/service/sync_service.h"

namespace apps {
namespace {

namespace cros_events = metrics::structured::events::v2::cr_os_events;

}  // namespace

AppDiscoveryMetrics::AppDiscoveryMetrics(
    Profile* profile,
    InstanceRegistry& instance_registry,
    AppPlatformMetrics* app_platform_metrics)
    : profile_(profile), app_platform_metrics_(app_platform_metrics) {
  DCHECK(app_platform_metrics);

  instance_registry_observation_.Observe(&instance_registry);
  app_platform_metrics_->AddObserver(this);
}

AppDiscoveryMetrics::~AppDiscoveryMetrics() {
  if (app_platform_metrics_) {
    app_platform_metrics_->RemoveObserver(this);
  }
}

void AppDiscoveryMetrics::OnAppInstalled(const std::string& app_id,
                                         AppType app_type,
                                         InstallSource app_install_source,
                                         InstallReason app_install_reason,
                                         InstallTime app_install_time) {
  // Do not record if app-sync is disabled.
  if (!IsAppSyncEnabled()) {
    return;
  }

  cros_events::AppDiscovery_AppInstalled event;
  event.SetAppId(GetAppStringToRecord(app_id, app_type))
      .SetAppType(static_cast<int>(app_type))
      .SetInstallSource(static_cast<int>(app_install_source))
      .SetInstallReason(static_cast<int>(app_install_reason));
  event.Record();
}

void AppDiscoveryMetrics::OnAppLaunched(const std::string& app_id,
                                        AppType app_type,
                                        LaunchSource launch_source) {
  // Do not record if app-sync is disabled.
  if (!IsAppSyncEnabled()) {
    return;
  }

  cros_events::AppDiscovery_AppLaunched event;
  event.SetAppId(GetAppStringToRecord(app_id, app_type))
      .SetAppType(static_cast<int>(app_type))
      .SetLaunchSource(static_cast<int>(launch_source));
  event.Record();
}

void AppDiscoveryMetrics::OnAppUninstalled(
    const std::string& app_id,
    AppType app_type,
    UninstallSource app_uninstall_source) {
  // Do not record if app-sync is disabled.
  if (!IsAppSyncEnabled()) {
    return;
  }

  cros_events::AppDiscovery_AppUninstall event;
  event.SetAppId(GetAppStringToRecord(app_id, app_type))
      .SetAppType(static_cast<int>(app_type))
      .SetUninstallSource(static_cast<int>(app_uninstall_source));
  event.Record();
}

void AppDiscoveryMetrics::OnAppPlatformMetricsDestroyed() {
  app_platform_metrics_ = nullptr;
}

void AppDiscoveryMetrics::OnInstanceUpdate(
    const InstanceUpdate& instance_update) {
  // No state changes. Ignore the update.
  if (!instance_update.StateChanged()) {
    return;
  }

  // Only record if app-sync is enabled. Recording is done before internal model
  // update to check for previous state.
  if (IsAppSyncEnabled()) {
    RecordAppState(instance_update);
  }

  // Apply state to internal model.

  // Instance is destroyed.
  if (instance_update.IsDestruction()) {
    instance_to_state_.erase(instance_update.InstanceId());
    app_id_to_instance_ids_[instance_update.AppId()].erase(
        instance_update.InstanceId());

    // If the set is now empty, all instances of the app are closed.
    // Remove app_id from the map and log app closed.
    if (app_id_to_instance_ids_[instance_update.AppId()].empty()) {
      app_id_to_instance_ids_.erase(instance_update.AppId());
    }

    return;
  }

  // First instance of the app requires set construction.
  if (app_id_to_instance_ids_.find(instance_update.AppId()) ==
      app_id_to_instance_ids_.end()) {
    app_id_to_instance_ids_[instance_update.AppId()] =
        std::set<base::UnguessableToken>();
  }

  app_id_to_instance_ids_[instance_update.AppId()].insert(
      instance_update.InstanceId());

  instance_to_state_[instance_update.InstanceId()] = instance_update.State();
}

void AppDiscoveryMetrics::OnInstanceRegistryWillBeDestroyed(
    InstanceRegistry* cache) {
  instance_registry_observation_.Reset();
}

bool AppDiscoveryMetrics::IsAppSyncEnabled() {
  return ShouldRecordUkm(profile_);
}

bool AppDiscoveryMetrics::IsAnyAppInstanceActive(
    const std::string& app_id,
    absl::optional<base::UnguessableToken> exclude_instance_id) {
  bool is_any_instance_active = false;

  // App id not found.
  if (app_id_to_instance_ids_.find(app_id) == app_id_to_instance_ids_.end()) {
    return false;
  }

  for (const auto& instance_id : app_id_to_instance_ids_[app_id]) {
    // Ignore excluded instance_id if it exists.
    if (IsStateActive(instance_to_state_[instance_id]) &&
        (!exclude_instance_id.has_value() ||
         exclude_instance_id.value() != instance_id)) {
      is_any_instance_active = true;
      break;
    }
  }

  return is_any_instance_active;
}

void AppDiscoveryMetrics::RecordAppState(
    const InstanceUpdate& instance_update) {
  if (instance_update.IsDestruction()) {
    RecordAppClosed(instance_update);
  } else if (IsUpdateActiveToInactive(instance_update)) {
    RecordAppInactive(instance_update);
  } else if (IsUpdateInactiveToActive(instance_update)) {
    RecordAppActive(instance_update);
  }
}

bool AppDiscoveryMetrics::IsUpdateActiveToInactive(
    const InstanceUpdate& instance_update) {
  const bool is_any_other_instance_active = IsAnyAppInstanceActive(
      instance_update.AppId(), instance_update.InstanceId());

  // kUnknown if no previous state exists.
  const InstanceState prev_state =
      instance_to_state_.find(instance_update.InstanceId()) ==
              instance_to_state_.end()
          ? InstanceState::kUnknown
          : instance_to_state_[instance_update.InstanceId()];

  // Active -> Inactive and no other instances are active. If no previous state,
  // then ignores the check for IsStateActive(prev_state)
  return (prev_state == InstanceState::kUnknown || IsStateActive(prev_state)) &&
         IsStateInactive(instance_update.State()) &&
         !is_any_other_instance_active;
}

bool AppDiscoveryMetrics::IsUpdateInactiveToActive(
    const InstanceUpdate& instance_update) {
  const bool is_any_other_instance_active = IsAnyAppInstanceActive(
      instance_update.AppId(), instance_update.InstanceId());

  // kUnknown if no previous state exists.
  const InstanceState prev_state =
      instance_to_state_.find(instance_update.InstanceId()) ==
              instance_to_state_.end()
          ? InstanceState::kUnknown
          : instance_to_state_[instance_update.InstanceId()];

  // Inactive -> Active and no other instances are active. If no previous state,
  // then ignores the check for IsStateInactive(prev_state)
  return (prev_state == InstanceState::kUnknown ||
          IsStateInactive(prev_state)) &&
         IsStateActive(instance_update.State()) &&
         !is_any_other_instance_active;
}

bool AppDiscoveryMetrics::IsStateInactive(InstanceState instance_state) {
  return (instance_state & InstanceState::kRunning) &&
         !IsStateActive(instance_state);
}

bool AppDiscoveryMetrics::IsStateActive(InstanceState instance_state) {
  return instance_state & InstanceState::kActive;
}

void AppDiscoveryMetrics::RecordAppActive(
    const InstanceUpdate& instance_update) {
  cros_events::AppDiscovery_AppStateChanged()
      .SetAppId(
          GetAppStringToRecord(instance_update.AppId(),
                               GetAppType(profile_, instance_update.AppId())))
      .SetAppState(static_cast<int>(AppStateChange::kActive))
      .Record();
}

void AppDiscoveryMetrics::RecordAppInactive(
    const InstanceUpdate& instance_update) {
  cros_events::AppDiscovery_AppStateChanged()
      .SetAppId(
          GetAppStringToRecord(instance_update.AppId(),
                               GetAppType(profile_, instance_update.AppId())))
      .SetAppState(static_cast<int>(AppStateChange::kInactive))
      .Record();
}

void AppDiscoveryMetrics::RecordAppClosed(
    const InstanceUpdate& instance_update) {
  DCHECK(instance_update.IsDestruction());
  auto prev_instances = app_id_to_instance_ids_[instance_update.AppId()];

  // If instance_update is the only instance of the app.
  if (prev_instances.size() == 1) {
    cros_events::AppDiscovery_AppStateChanged()
        .SetAppId(
            GetAppStringToRecord(instance_update.AppId(),
                                 GetAppType(profile_, instance_update.AppId())))
        .SetAppState(static_cast<int>(AppStateChange::kClosed))
        .Record();
  }
}

std::string AppDiscoveryMetrics::GetAppStringToRecord(
    const std::string& hashed_app_id,
    AppType app_type) {
  // Generates a URL for the given |profile_| and |hashed_app_id| that may
  // return the canonical app name for certain |app_type|.
  GURL url = AppPlatformMetrics::GetURLForApp(profile_, hashed_app_id);

  switch (app_type) {
    // Collects the app package name unhashed. App package names are public and
    // in the play store.
    case AppType::kArc:
      return url.spec();

    // Crostini apps are identified by concatenating the desktop_id and the
    // app_id. For more documentation as to what the desktop_id and app_id,
    // refer to the documentation in
    // //chrome/browser/ash/guest_os/guest_os_registery_service.h.
    case AppType::kCrostini:

    // Borealis apps are identified using the steam app ID, which is a number
    // assigned to games by steam.
    case AppType::kBorealis:
      return url.spec();

    // Web apps may contain sensitive data in the URLs. Collect the
    // |hashed_app_id| of the app to avoid collecting potentially sensitive data
    // in the URL.
    case AppType::kWeb:
      return hashed_app_id;

    // These app types have app names that are hashed before the URLs are
    // generated.
    case AppType::kBuiltIn:
    case AppType::kChromeApp:
    case AppType::kExtension:
    case AppType::kStandaloneBrowser:
    case AppType::kStandaloneBrowserChromeApp:
    case AppType::kStandaloneBrowserExtension:
    case AppType::kSystemWeb:
      return url.spec();

    // Any other app types should not be collected.
    default:
      return "";
  }
}

}  // namespace apps
