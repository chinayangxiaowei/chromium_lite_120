// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/android/permission_prompt/permission_prompt_android.h"

#include <vector>

#include "components/permissions/features.h"
#include "components/permissions/permission_request.h"
#include "components/resources/android/theme_resources.h"
#include "components/strings/grit/components_strings.h"
#include "components/url_formatter/elide_url.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/strings/grit/ui_strings.h"

namespace permissions {

PermissionPromptAndroid::PermissionPromptAndroid(
    content::WebContents* web_contents,
    Delegate* delegate)
    : web_contents_(web_contents), delegate_(delegate) {
  DCHECK(web_contents);
}

PermissionPromptAndroid::~PermissionPromptAndroid() = default;

bool PermissionPromptAndroid::UpdateAnchor() {
  NOTIMPLEMENTED();
  return false;
}

PermissionPrompt::TabSwitchingBehavior
PermissionPromptAndroid::GetTabSwitchingBehavior() {
  return TabSwitchingBehavior::kKeepPromptAlive;
}

absl::optional<gfx::Rect> PermissionPromptAndroid::GetViewBoundsInScreen()
    const {
  return absl::nullopt;
}

bool PermissionPromptAndroid::ShouldFinalizeRequestAfterDecided() const {
  return true;
}

void PermissionPromptAndroid::Closing() {
  delegate_->Dismiss();
}

void PermissionPromptAndroid::Accept() {
  delegate_->Accept();
}

void PermissionPromptAndroid::Deny() {
  delegate_->Deny();
}

void PermissionPromptAndroid::SetManageClicked() {
  delegate_->SetManageClicked();
}

void PermissionPromptAndroid::SetLearnMoreClicked() {
  delegate_->SetLearnMoreClicked();
}

bool PermissionPromptAndroid::ShouldCurrentRequestUseQuietUI() {
  return delegate_->ShouldCurrentRequestUseQuietUI();
}

absl::optional<PermissionUiSelector::QuietUiReason>
PermissionPromptAndroid::ReasonForUsingQuietUi() const {
  return delegate_->ReasonForUsingQuietUi();
}

size_t PermissionPromptAndroid::PermissionCount() const {
  return delegate_->Requests().size();
}

ContentSettingsType PermissionPromptAndroid::GetContentSettingType(
    size_t position) const {
  const std::vector<PermissionRequest*>& requests = delegate_->Requests();
  CHECK_LT(position, requests.size());
  return requests[position]->GetContentSettingsType();
}

static bool IsValidMediaRequestGroup(
    const std::vector<PermissionRequest*>& requests) {
  if (requests.size() < 2)
    return false;
  return ((requests[0]->request_type() == RequestType::kMicStream &&
           requests[1]->request_type() == RequestType::kCameraStream) ||
          (requests[0]->request_type() == RequestType::kCameraStream &&
           requests[1]->request_type() == RequestType::kMicStream));
}

// Grouped permission requests can only be Mic+Camera, Camera+Mic.
static void CheckValidRequestGroup(
    const std::vector<PermissionRequest*>& requests) {
  DCHECK_EQ(static_cast<size_t>(2u), requests.size());
  DCHECK((IsValidMediaRequestGroup(requests)));
}

int PermissionPromptAndroid::GetIconId() const {
  const std::vector<PermissionRequest*>& requests = delegate_->Requests();
  if (requests.size() == 1) {
    if (requests[0]->request_type() == RequestType::kStorageAccess &&
        base::FeatureList::IsEnabled(
            permissions::features::kPermissionStorageAccessAPI)) {
      return IDR_ANDROID_GLOBE;
    }
    return permissions::GetIconId(requests[0]->request_type());
  }
  CheckValidRequestGroup(requests);
  return IDR_ANDROID_INFOBAR_MEDIA_STREAM_CAMERA;
}

std::u16string PermissionPromptAndroid::GetMessageText() const {
  const std::vector<PermissionRequest*>& requests = delegate_->Requests();
  if (requests.size() == 1) {
    if (requests[0]->request_type() == RequestType::kStorageAccess) {
      if (base::FeatureList::IsEnabled(
              permissions::features::kPermissionStorageAccessAPI)) {
        auto requesting_origin = url_formatter::FormatUrlForSecurityDisplay(
            delegate_->GetRequestingOrigin(),
            url_formatter::SchemeDisplay::OMIT_CRYPTOGRAPHIC);
        auto embedding_origin = url_formatter::FormatUrlForSecurityDisplay(
            delegate_->GetEmbeddingOrigin(),
            url_formatter::SchemeDisplay::OMIT_CRYPTOGRAPHIC);

        return l10n_util::GetStringFUTF16(
            IDS_CONCAT_TWO_STRINGS_WITH_PERIODS,
            l10n_util::GetStringFUTF16(
                IDS_STORAGE_ACCESS_PERMISSION_TWO_ORIGIN_PROMPT_TITLE,
                requesting_origin),
            l10n_util::GetStringFUTF16(
                IDS_STORAGE_ACCESS_PERMISSION_TWO_ORIGIN_EXPLANATION,
                requesting_origin, embedding_origin));
      }
      return l10n_util::GetStringFUTF16(
          IDS_STORAGE_ACCESS_INFOBAR_TEXT,
          url_formatter::FormatUrlForSecurityDisplay(
              delegate_->GetRequestingOrigin(),
              url_formatter::SchemeDisplay::OMIT_CRYPTOGRAPHIC),
          url_formatter::FormatUrlForSecurityDisplay(
              delegate_->GetEmbeddingOrigin(),
              url_formatter::SchemeDisplay::OMIT_CRYPTOGRAPHIC));
    }
    return requests[0]->GetDialogMessageText();
  }
  CheckValidRequestGroup(requests);
  return l10n_util::GetStringFUTF16(
      IDS_MEDIA_CAPTURE_AUDIO_AND_VIDEO_INFOBAR_TEXT,
      url_formatter::FormatUrlForSecurityDisplay(
          delegate_->GetRequestingOrigin(),
          url_formatter::SchemeDisplay::OMIT_CRYPTOGRAPHIC));
}

bool PermissionPromptAndroid::ShouldUseRequestingOriginFavicon() const {
  const std::vector<PermissionRequest*>& requests = delegate_->Requests();
  CHECK_GT(requests.size(), 0U);

  return requests[0]->request_type() == RequestType::kStorageAccess &&
         base::FeatureList::IsEnabled(
             permissions::features::kPermissionStorageAccessAPI);
}

GURL PermissionPromptAndroid::GetRequestingOrigin() const {
  return delegate_->GetRequestingOrigin();
}

}  // namespace permissions
