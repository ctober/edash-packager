// Copyright 2014 Google Inc. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "packager/media/event/mpd_notify_muxer_listener.h"

#include <cmath>

#include "packager/base/logging.h"
#include "packager/media/base/audio_stream_info.h"
#include "packager/media/base/video_stream_info.h"
#include "packager/media/event/muxer_listener_internal.h"
#include "packager/mpd/base/media_info.pb.h"
#include "packager/mpd/base/mpd_notifier.h"

namespace edash_packager {
namespace media {
namespace event {

MpdNotifyMuxerListener::MpdNotifyMuxerListener(MpdNotifier* mpd_notifier)
    : mpd_notifier_(mpd_notifier), notification_id_(0) {
  DCHECK(mpd_notifier);
  DCHECK(mpd_notifier->dash_profile() == kOnDemandProfile ||
         mpd_notifier->dash_profile() == kLiveProfile);
}

MpdNotifyMuxerListener::~MpdNotifyMuxerListener() {}

void MpdNotifyMuxerListener::SetContentProtectionSchemeIdUri(
    const std::string& scheme_id_uri) {
  scheme_id_uri_ = scheme_id_uri;
}

void MpdNotifyMuxerListener::OnMediaStart(
    const MuxerOptions& muxer_options,
    const std::vector<StreamInfo*>& stream_infos,
    uint32_t time_scale,
    ContainerType container_type,
    bool is_encrypted) {
  scoped_ptr<MediaInfo> media_info(new MediaInfo());
  if (!internal::GenerateMediaInfo(muxer_options,
                                   stream_infos,
                                   time_scale,
                                   container_type,
                                   media_info.get())) {
    LOG(ERROR) << "Failed to generate MediaInfo from input.";
    return;
  }

  if (is_encrypted) {
    if (!internal::AddContentProtectionElements(
            container_type, scheme_id_uri_, media_info.get())) {
      LOG(ERROR) << "Failed to add content protection elements.";
      return;
    }
  }

  if (mpd_notifier_->dash_profile() == kLiveProfile) {
    // TODO(kqyang): Check return result.
    mpd_notifier_->NotifyNewContainer(*media_info, &notification_id_);
  } else {
    media_info_ = media_info.Pass();
  }
}

void MpdNotifyMuxerListener::OnMediaEnd(bool has_init_range,
                                        uint64_t init_range_start,
                                        uint64_t init_range_end,
                                        bool has_index_range,
                                        uint64_t index_range_start,
                                        uint64_t index_range_end,
                                        float duration_seconds,
                                        uint64_t file_size) {
  if (mpd_notifier_->dash_profile() == kLiveProfile) return;

  DCHECK(media_info_);
  if (!internal::SetVodInformation(has_init_range,
                                   init_range_start,
                                   init_range_end,
                                   has_index_range,
                                   index_range_start,
                                   index_range_end,
                                   duration_seconds,
                                   file_size,
                                   media_info_.get())) {
    LOG(ERROR) << "Failed to generate VOD information from input.";
    return;
  }

  uint32_t id;  // Result unused.
  // TODO(kqyang): Check return result.
  mpd_notifier_->NotifyNewContainer(*media_info_, &id);
}

void MpdNotifyMuxerListener::OnNewSegment(uint64_t start_time,
                                          uint64_t duration,
                                          uint64_t segment_file_size) {
  if (mpd_notifier_->dash_profile() != kLiveProfile) return;
  // TODO(kqyang): Check return result.
  mpd_notifier_->NotifyNewSegment(
      notification_id_, start_time, duration, segment_file_size);
}

}  // namespace event
}  // namespace media
}  // namespace edash_packager
