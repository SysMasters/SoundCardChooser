//
// Created by rendedong on 2023/8/2.
//

#include "SoundPlayStrategy8L.h"
#include "../../Player.h"

SoundPlayStrategy8L::SoundPlayStrategy8L(Player *player) : SoundPlayStrategy(player) {}

SoundPlayStrategy8L::~SoundPlayStrategy8L() {
}

int SoundPlayStrategy8L::playSound() {
    LOGI("播放策略：817左\n");
    if (!playerPtr->pcm_in) {
        //打开声卡
        playerPtr->pcm_in = pcm_open(3, 0, PCM_IN, &playerPtr->config);
        if (!playerPtr->pcm_in || !pcm_is_ready(playerPtr->pcm_in)) {
            LOGE("Unable to open PCM device (%s)\n", pcm_get_error(playerPtr->pcm_in));
            if (playerPtr->jniCallbackHelper) {
                playerPtr->jniCallbackHelper->onError(THREAD_CHILD, ERROR_OPEN_PCMC0D0C_FAIL);
            }
            return PLAY_FAIL;
        } else {
            LOGI("pcmC0D0c打开啦\n");
        }
    } else {
        LOGI("pcmC0D0c之前已结打开啦\n");
    }
    LOGD("一个周期内有多少采样点：%u\n", pcm_get_buffer_size(playerPtr->pcm_in));

    if (!playerPtr->size) {
        //获取buffer的大小
        playerPtr->size = pcm_frames_to_bytes(playerPtr->pcm_in, pcm_get_buffer_size(
                playerPtr->pcm_in));
    }
    LOGD("一个周期内占用多少字节：%u\n", playerPtr->size);

    //初始化buffer
    if (!playerPtr->buffer) {
        playerPtr->buffer = static_cast<char *>(malloc(playerPtr->size));
        if (!playerPtr->buffer) {
            LOGE("pcmC0D0c的buffer:Unable to allocate %u bytes\n", playerPtr->size);
            if (playerPtr->jniCallbackHelper) {
                playerPtr->jniCallbackHelper->onError(THREAD_CHILD,
                                                      ERROR_INIT_PCMC0D0C_BUFFER_FAIL);
            }
            return PLAY_FAIL;
        } else {
            LOGI("pcmC0D0c的buffer初始化啦\n");
        }
    } else {
        LOGI("pcmC0D0c的buffer之前已结初始化啦\n");
    }

    LOGI("Capturing sample: %u ch, %u hz, %u bit\n", 2, 44100,
         pcm_format_to_bits(PCM_FORMAT_S16_LE));

    playerPtr->status = STATUS_PLAYING;

    //开始播放
    //只有当结束播放、重置、声卡文件读取数据出错时才会结束循环（结束播放）
    while (playerPtr->status != STATUS_COMPLETE && playerPtr->status != STATUS_UNPLAY &&
           !pcm_read(playerPtr->pcm_in, playerPtr->buffer, playerPtr->size)) {

        //锁的用法
        pthread_mutex_lock(&playerPtr->mutex);
        if (playerPtr->status == STATUS_PAUSE) {
            //线程等待，直到有其他线程唤醒它
            pthread_cond_wait(&playerPtr->cond, &playerPtr->mutex);
        }
        pthread_mutex_unlock(&playerPtr->mutex);


        //打印字符数组
        std::string output2;
        for (int i = 0; i < playerPtr->size; ++i) {
            output2 += std::to_string(static_cast<unsigned char>(*(playerPtr->buffer + i))) + " ";
        }
        LOGD("817取左声道前：%s\n", output2.c_str());


        //算法：快慢指针
        //3,4复制1，2。
        //声卡是817时：
        //817是左声道录制的听诊音，所以这里等于是将听诊音复制到了右声道，可以播放成功。
        int i = 0;
        int j = 2;
        int n = playerPtr->size / 4;
        for (int k = 1; k <= n; ++k) {
            playerPtr->buffer[j] = playerPtr->buffer[i];
            playerPtr->buffer[j + 1] = playerPtr->buffer[i + 1];
            i += 4;
            j += 4;
        }


        if (playerPtr->jniCallbackHelper) {
            //将录音buffer传给给java层播放
            playerPtr->jniCallbackHelper->onCallback(playerPtr->buffer, playerPtr->size);
        } else {
            LOGE("未初始化播放回调\n");
            return PLAY_FAIL;
        }

        std::string output;
        for (int i = 0; i < playerPtr->size; ++i) {
            output += std::to_string(static_cast<unsigned char>(*(playerPtr->buffer + i))) + " ";
        }
        LOGD("817取左声道后：%s\n", output.c_str());


        //保存录音buffer到文件
        if (fwrite(playerPtr->buffer, 1, playerPtr->size, playerPtr->file) != playerPtr->size) {
            LOGE("边录边播时：向保存文件写入音频数据失败\n");
            if (playerPtr->jniCallbackHelper) {
                playerPtr->jniCallbackHelper->onError(THREAD_CHILD, ERROR_SAVE_AUDIO_FAIL);
            }
            return PLAY_FAIL;
        }
        playerPtr->bytes_read += playerPtr->size;

    }

    //只有当结束后状态是STATUS_UNPLAY（重置）或STATUS_COMPLETE（完成）时，才是正常结束，否则就是播放的过程中出问题了
    if (playerPtr->status != STATUS_UNPLAY && playerPtr->status != STATUS_COMPLETE) {
        LOGE("播放过程中：pcmC0D0c数据读取错误\n");
        if (playerPtr->jniCallbackHelper) {
            playerPtr->jniCallbackHelper->onError(THREAD_CHILD, ERROR_READ_PCMC0D0C_FAIL);
        }
        return PLAY_FAIL;
    }

    //正常结束后，如果是重置，那么就重置资源；如果是完成，那么就保存录音文件（写入wav头信息），并跳转页面
    playerPtr->afterPlay(playerPtr->pcm_in);

    return PLAY_SUCCESS;

}