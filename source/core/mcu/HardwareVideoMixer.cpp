/*
 * Copyright 2014 Intel Corporation All Rights Reserved. 
 * 
 * The source code contained or described herein and all documents related to the 
 * source code ("Material") are owned by Intel Corporation or its suppliers or 
 * licensors. Title to the Material remains with Intel Corporation or its suppliers 
 * and licensors. The Material contains trade secrets and proprietary and 
 * confidential information of Intel or its suppliers and licensors. The Material 
 * is protected by worldwide copyright and trade secret laws and treaty provisions. 
 * No part of the Material may be used, copied, reproduced, modified, published, 
 * uploaded, posted, transmitted, distributed, or disclosed in any way without 
 * Intel's prior express written permission.
 * 
 * No license under any patent, copyright, trade secret or other intellectual 
 * property right is granted to or conferred upon you by disclosure or delivery of 
 * the Materials, either expressly, by implication, inducement, estoppel or 
 * otherwise. Any license under such intellectual property rights must be express 
 * and approved by Intel in writing.
 */

#include "HardwareVideoMixer.h"

namespace mcu {

CodecType Frameformat2CodecType(FrameFormat format)
{
    switch (format) {
        case FRAME_FORMAT_H264:
            return ::CODEC_TYPE_VIDEO_AVC;
        case FRAME_FORMAT_VP8:
            return ::CODEC_TYPE_VIDEO_VP8;
        default:
            return ::CODEC_TYPE_INVALID;
    }
}

HardwareVideoMixerInput::HardwareVideoMixerInput(boost::shared_ptr<VideoMixEngine> engine,
                                                 FrameFormat inFormat, 
                                                 VideoMixInProvider* provider)
    : m_index(INVALID_INPUT_INDEX)
    , m_provider(provider)
    , m_engine(engine)
{
    assert((inFormat == FRAME_FORMAT_VP8 || inFormat == FRAME_FORMAT_H264) && m_provider != NULL);

    m_index = m_engine->EnableInput(Frameformat2CodecType(inFormat), this);
}

HardwareVideoMixerInput::~HardwareVideoMixerInput()
{
    if(m_index != INVALID_INPUT_INDEX && m_engine.get()) {
        m_engine->DisableInput(m_index);
        m_index = INVALID_INPUT_INDEX;
    }
}

void HardwareVideoMixerInput::push(unsigned char* payload, int len)
{
    m_engine->PushInput(m_index, payload, len);
}

void HardwareVideoMixerInput::requestKeyFrame(InputIndex index) {
    if (index == m_index) {
        m_provider->requestKeyFrame();
    }
}

HardwareVideoMixerOutput::HardwareVideoMixerOutput(boost::shared_ptr<VideoMixEngine> engine,
                                                    FrameFormat outFormat,
                                                    std::string name,
                                                    unsigned int framerate,
                                                    unsigned short bitrate,
                                                    VideoMixOutConsumer* receiver)
    : m_outFormat(outFormat)
    , m_index(INVALID_OUTPUT_INDEX)
    , m_engine(engine)
    , m_receiver(receiver)
    , m_frameRate(framerate)
    , m_outCount(0)
{
    assert((m_outFormat == FRAME_FORMAT_VP8 || m_outFormat == FRAME_FORMAT_H264) && m_receiver != NULL);

    m_index = m_engine->EnableOutput(Frameformat2CodecType(m_outFormat), name.c_str(), bitrate, this);
    assert(m_index != INVALID_OUTPUT_INDEX);
    m_jobTimer.reset(new JobTimer(m_frameRate, this));
}

HardwareVideoMixerOutput::~HardwareVideoMixerOutput()
{
    if(m_index != INVALID_OUTPUT_INDEX && m_engine.get()) {
        m_engine->DisableOutput(m_index);
        m_index = INVALID_OUTPUT_INDEX;
    }
}

void HardwareVideoMixerOutput::setBitrate(unsigned short bitrate)
{
    m_engine->SetBitrate(m_index, bitrate);
}

void HardwareVideoMixerOutput::requestKeyFrame()
{
    m_engine->RequestKeyFrame(m_index);
}

void HardwareVideoMixerOutput::onTimeout()
{
    int len = m_engine->PullOutput(m_index, (unsigned char*)&m_esBuffer);
    if (len > 0) {
        m_outCount++;
        m_receiver->onFrame(m_outFormat, (unsigned char *)&m_esBuffer, len, m_outCount * m_frameRate);
    }
}

void HardwareVideoMixerOutput::notifyFrameReady(OutputIndex index)
{
    if (index == m_index) {
        // TODO: Pulling stratege (onTigger) is used now, but if mix-engine support notify mechanism, we
        // can receive output frame by this method.
    }
}

HardwareVideoMixer::HardwareVideoMixer()
{
    m_engine.reset(new VideoMixEngine());
    // assert(m_engine->Init());
}

HardwareVideoMixer::~HardwareVideoMixer()
{}

void HardwareVideoMixer::setBitrate(FrameFormat format, unsigned short bitrate)
{
    std::map<FrameFormat, boost::shared_ptr<HardwareVideoMixerOutput>>::iterator it = m_outputs.find(format);
    if (it != m_outputs.end())
        it->second->setBitrate(bitrate);
}

void HardwareVideoMixer::requestKeyFrame(FrameFormat format)
{
    std::map<FrameFormat, boost::shared_ptr<HardwareVideoMixerOutput>>::iterator it = m_outputs.find(format);
    if (it != m_outputs.end())
        it->second->requestKeyFrame();
}

void HardwareVideoMixer::setLayout(struct VideoLayout& layout)
{
    //TODO: set the layout inFormation to engine.
    //m_engine->setLayout(layout);
}

bool HardwareVideoMixer::activateInput(int slot, FrameFormat format, VideoMixInProvider* provider)
{
    if (m_inputs.find(slot) != m_inputs.end()) {
        return false;
    } else {
        m_inputs[slot].reset(new HardwareVideoMixerInput(m_engine, format, provider));
        return true;
    }
}

void HardwareVideoMixer::deActivateInput(int slot)
{
    m_inputs.erase(slot);
}

void HardwareVideoMixer::pushInput(int slot, unsigned char* payload, int len)
{
    std::map<int, boost::shared_ptr<HardwareVideoMixerInput>>::iterator it = m_inputs.find(slot);
    if (it != m_inputs.end())
        it->second->push(payload, len);
}

// Should be refined when VCSA supports init().
bool HardwareVideoMixer::activateOutput(FrameFormat format, unsigned int framerate, unsigned short bitrate, VideoMixOutConsumer* receiver)
{
    if (m_outputs.find(format) != m_outputs.end()) {
        return false;
    } else {
        m_outputs[format].reset(new HardwareVideoMixerOutput(m_engine, format, "name", framerate, bitrate, receiver));
        return true;
    }
}

void HardwareVideoMixer::deActivateOutput(FrameFormat format)
{
    m_outputs.erase(format);
}

}