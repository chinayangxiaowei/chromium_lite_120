// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/recovery_eligibility_screen.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "base/functional/callback.h"
#include "chrome/browser/ash/login/wizard_context.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/webui/ash/login/recovery_eligibility_screen_handler.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"
#include "chromeos/ash/components/login/auth/recovery/recovery_utils.h"
#include "chromeos/ash/components/osauth/public/auth_session_storage.h"
#include "components/prefs/pref_service.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash {

namespace {

// Returns `true` if the active Profile is enterprise managed.
bool IsUserEnterpriseManaged() {
  Profile* profile = ProfileManager::GetActiveUserProfile();
  return profile->GetProfilePolicyConnector()->IsManaged() &&
         !profile->IsChild();
}

}  // namespace

// static
std::string RecoveryEligibilityScreen::GetResultString(Result result) {
  switch (result) {
    case Result::PROCEED:
      return "Proceed";
    case Result::NOT_APPLICABLE:
      return BaseScreen::kNotApplicable;
  }
}

// static
bool RecoveryEligibilityScreen::ShouldSkipRecoverySetupBecauseOfPolicy() {
  return IsUserEnterpriseManaged() &&
         !GetRecoveryDefaultState(
             IsUserEnterpriseManaged(),
             ProfileManager::GetActiveUserProfile()->GetPrefs());
}

RecoveryEligibilityScreen::RecoveryEligibilityScreen(
    const ScreenExitCallback& exit_callback)
    : BaseScreen(RecoveryEligibilityView::kScreenId,
                 OobeScreenPriority::DEFAULT),
      exit_callback_(exit_callback) {}

RecoveryEligibilityScreen::~RecoveryEligibilityScreen() = default;

bool RecoveryEligibilityScreen::MaybeSkip(WizardContext& wizard_context) {
  if (!features::IsCryptohomeRecoveryEnabled()) {
    exit_callback_.Run(Result::NOT_APPLICABLE);
    return true;
  }
  std::unique_ptr<UserContext> user_context;
  if (ash::features::ShouldUseAuthSessionStorage()) {
    if (!wizard_context.extra_factors_token.has_value()) {
      exit_callback_.Run(Result::NOT_APPLICABLE);
      return true;
    }
    if (!ash::AuthSessionStorage::Get()->IsValid(
            wizard_context.extra_factors_token.value())) {
      exit_callback_.Run(Result::NOT_APPLICABLE);
      return true;
    }
  } else {
    if (!wizard_context.extra_factors_auth_session.get()) {
      exit_callback_.Run(Result::NOT_APPLICABLE);
      return true;
    }
  }

  if (wizard_context.skip_post_login_screens_for_tests) {
    exit_callback_.Run(Result::NOT_APPLICABLE);
    return true;
  }
  return false;
}

void RecoveryEligibilityScreen::ShowImpl() {
  const UserContext* user_context = nullptr;
  if (ash::features::ShouldUseAuthSessionStorage()) {
    CHECK(context()->extra_factors_token.has_value());
    auto* storage = ash::AuthSessionStorage::Get();
    auto& token = context()->extra_factors_token.value();
    CHECK(storage->IsValid(token));
    user_context = storage->Peek(token);
  } else {
    user_context = context()->extra_factors_auth_session.get();
  }

  CHECK(user_context);
  auto supported_factors =
      user_context->GetAuthFactorsConfiguration().get_supported_factors();
  if (supported_factors.Has(cryptohome::AuthFactorType::kRecovery)) {
    context()->recovery_setup.is_supported = true;
    // Don't ask about recovery consent for managed users - use the policy value
    // instead.
    context()->recovery_setup.ask_about_recovery_consent =
        IsRecoveryOptInAvailable(IsUserEnterpriseManaged());
    context()->recovery_setup.recovery_factor_opted_in =
        GetRecoveryDefaultState(
            IsUserEnterpriseManaged(),
            ProfileManager::GetActiveUserProfile()->GetPrefs());
  }
  exit_callback_.Run(Result::PROCEED);
}

void RecoveryEligibilityScreen::HideImpl() {}

}  // namespace ash
