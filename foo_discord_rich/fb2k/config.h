#pragma once

#include <qwr/fb2k_config.h>

namespace drp::config
{

enum class ImageSetting : uint8_t
{
    Light = 0,
    Dark,
    Disabled
};
enum class TimeSetting : uint8_t
{
    ProgressBar = 0,  // Shows progress bar (start + end timestamps)
    Elapsed = 1,      // Shows elapsed (start timestamp, old display mode)
    Disabled = 2      // Discord decides, i.e. elapsed since last status update, won't follow playback unless never interacted with
};
enum class StatusSetting : uint8_t
{
    Name = 0,
    Middle,     // to match order with discord's "name, state, details"
    Top
};

extern qwr::fb2k::ConfigBool isEnabled;
extern qwr::fb2k::ConfigUint8Enum<ImageSetting> largeImageSettings;
extern qwr::fb2k::ConfigUint8Enum<ImageSetting> smallImageSettings;
extern qwr::fb2k::ConfigUint8Enum<TimeSetting> timeSettings;
extern qwr::fb2k::ConfigUint8Enum<StatusSetting> statusSettings;
extern qwr::fb2k::ConfigString stateQuery;
extern qwr::fb2k::ConfigString detailsQuery;

extern qwr::fb2k::ConfigString discordAppToken;
extern qwr::fb2k::ConfigString largeImageId_Light;
extern qwr::fb2k::ConfigString largeImageId_Dark;
extern qwr::fb2k::ConfigString playingImageId_Light;
extern qwr::fb2k::ConfigString playingImageId_Dark;
extern qwr::fb2k::ConfigString pausedImageId_Dark;
extern qwr::fb2k::ConfigString pausedImageId_Light;
extern qwr::fb2k::ConfigString uploadArtworkCommand;
extern qwr::fb2k::ConfigString artworkMetadbKey;
extern qwr::fb2k::ConfigUint32 processTimeout;

extern qwr::fb2k::ConfigBool disableWhenPaused;
extern qwr::fb2k::ConfigBool swapSmallImages;
extern qwr::fb2k::ConfigBool uploadArtwork;

}; // namespace drp::config
