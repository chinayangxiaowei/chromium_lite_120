// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/metrics/model/web_state_list_metrics_browser_agent.h"

#import "base/metrics/histogram_functions.h"
#import "base/metrics/histogram_macros.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "components/navigation_metrics/navigation_metrics.h"
#import "components/profile_metrics/browser_profile_type.h"
#import "ios/chrome/browser/crash_report/model/crash_loop_detection_util.h"
#import "ios/chrome/browser/sessions/session_restoration_observer_helper.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/model/web_state_list/all_web_state_observation_forwarder.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/web_state_list/session_metrics.h"
#import "ios/web/public/browser_state.h"
#import "ios/web/public/navigation/navigation_context.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/web_state.h"

BROWSER_USER_DATA_KEY_IMPL(WebStateListMetricsBrowserAgent)

WebStateListMetricsBrowserAgent::WebStateListMetricsBrowserAgent(
    Browser* browser,
    SessionMetrics* session_metrics)
    : web_state_list_(browser->GetWebStateList()),
      session_metrics_(session_metrics) {
  DCHECK(web_state_list_);
  DCHECK(session_metrics_);
  browser->AddObserver(this);
  web_state_list_->AddObserver(this);
  web_state_forwarder_ =
      std::make_unique<AllWebStateObservationForwarder>(web_state_list_, this);

  AddSessionRestorationObserver(browser, this);
}

WebStateListMetricsBrowserAgent::~WebStateListMetricsBrowserAgent() = default;

#pragma mark - SessionRestorationObserver

void WebStateListMetricsBrowserAgent::WillStartSessionRestoration(
    Browser* browser) {
  // Ignore the event if it does not correspond to the browser this
  // object is bound to (which can happen with the optimised session
  // storage code).
  if (browser->GetWebStateList() != web_state_list_) {
    return;
  }

  metric_collection_paused_ = true;
}

void WebStateListMetricsBrowserAgent::SessionRestorationFinished(
    Browser* browser,
    const std::vector<web::WebState*>& restored_web_states) {
  // Ignore the event if it does not correspond to the browser this
  // object is bound to (which can happen with the optimised session
  // storage code).
  if (browser->GetWebStateList() != web_state_list_) {
    return;
  }

  metric_collection_paused_ = false;
}

#pragma mark - WebStateListObserver

void WebStateListMetricsBrowserAgent::WebStateListWillChange(
    WebStateList* web_state_list,
    const WebStateListChangeDetach& detach_change,
    const WebStateListStatus& status) {
  if (!detach_change.is_closing()) {
    return;
  }

  if (metric_collection_paused_) {
    return;
  }

  base::TimeDelta age_at_deletion =
      base::Time::Now() - detach_change.detached_web_state()->GetCreationTime();
  base::UmaHistogramCustomTimes("Tab.AgeAtDeletion", age_at_deletion,
                                base::Minutes(1), base::Days(24), 50);

  if (detach_change.is_user_action()) {
    base::RecordAction(base::UserMetricsAction("MobileTabClosed"));
  }
}

void WebStateListMetricsBrowserAgent::WebStateListDidChange(
    WebStateList* web_state_list,
    const WebStateListChange& change,
    const WebStateListStatus& status) {
  if (metric_collection_paused_) {
    return;
  }

  if (change.type() == WebStateListChange::Type::kInsert) {
    base::RecordAction(base::UserMetricsAction("MobileNewTabOpened"));
  }

  if (status.active_web_state_change()) {
    session_metrics_->OnWebStateActivated();
    if (change.type() == WebStateListChange::Type::kReplace) {
      return;
    }

    base::RecordAction(base::UserMetricsAction("MobileTabSwitched"));
  }
}

#pragma mark - WebStateObserver

void WebStateListMetricsBrowserAgent::DidFinishNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  if (!navigation_context->HasCommitted()) {
    return;
  }

  if (!navigation_context->IsSameDocument() &&
      !web_state->GetBrowserState()->IsOffTheRecord()) {
    int tab_count = static_cast<int>(web_state_list_->count());
    UMA_HISTOGRAM_CUSTOM_COUNTS("Tabs.TabCountPerLoad", tab_count, 1, 200, 50);
  }

  web::NavigationItem* item =
      web_state->GetNavigationManager()->GetLastCommittedItem();
  navigation_metrics::RecordPrimaryMainFrameNavigation(
      item ? item->GetVirtualURL() : GURL::EmptyGURL(),
      navigation_context->IsSameDocument(),
      web_state->GetBrowserState()->IsOffTheRecord(),
      profile_metrics::GetBrowserProfileType(web_state->GetBrowserState()));
}

void WebStateListMetricsBrowserAgent::PageLoaded(
    web::WebState* web_state,
    web::PageLoadCompletionStatus load_completion_status) {
  switch (GetInterfaceOrientation()) {
    case UIInterfaceOrientationPortrait:
    case UIInterfaceOrientationPortraitUpsideDown:
      UMA_HISTOGRAM_BOOLEAN("Tab.PageLoadInPortrait", YES);
      break;
    case UIInterfaceOrientationLandscapeLeft:
    case UIInterfaceOrientationLandscapeRight:
      UMA_HISTOGRAM_BOOLEAN("Tab.PageLoadInPortrait", NO);
      break;
    case UIInterfaceOrientationUnknown:
      // TODO(crbug.com/228832): Convert from a boolean histogram to an
      // enumerated histogram and log this case as well.
      break;
  }
  base::UmaHistogramBoolean("Tab.HasThemeColor",
                            web_state->GetThemeColor() != nil);
}

#pragma mark - BrowserObserver

void WebStateListMetricsBrowserAgent::BrowserDestroyed(Browser* browser) {
  DCHECK_EQ(browser->GetWebStateList(), web_state_list_);

  RemoveSessionRestorationObserver(browser, this);

  web_state_forwarder_.reset(nullptr);
  web_state_list_->RemoveObserver(this);
  web_state_list_ = nullptr;

  browser->RemoveObserver(this);
}
