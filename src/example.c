/*
	example:　传音频到dui服务器做对话

	从dui服务端获取到对话结果后，会依次抛出：
	DDS_EV_OUT_ASR_RESULT 	识别结果
	DDS_EV_OUT_DUI_RESPONSE	dui服务器返回的对话结果（json格式）
	DDS_EV_OUT_TTS　			对话回复对应的合成音链接，从这个链接下载音频

	初始化后，dds状态为idle
	发送DDS_EV_IN_SPEECH "start"后，dds状态转换为listening，此时将音频送入dds.
	发送DDS_EV_IN_SPEECH "end"后，dds状态转换为understanging，等待dui服务器返回结果。
	获取到dui服务器结果后，如果是单轮对话，dds状态转换为idle，否则为listening。
	进入下一次对话，重复上述操作

	测试说法：
	苏州今天的天气怎么样	天气技能，单轮对话
	你叫什么名字			闲聊技能，多轮对话
	
*/
static const char *productId = "279608534";
static const char *aliasKey = "test";
static const char *deviceProfile = "0KOe9Z/cU5Pe9y7raUY4ZptgS2iGxYTp6yS9ra1SF5ONjIwSJe7s24TdnpOTkIjdxYuNiprT3ZuaiZacmraRmZDdxYTdnJeWj7KQm5qT3cXdh4eHh93T3Y+NkJuKnIu2m93F3c3IxsnPx8rMy93T3ZaRjIuNipyLlpCRrJqL3cXdno2Sicjd092bmomWnJqxnpKa3cXdyceayJ3HzM7Sz8/OmtLMyJ7I0p3Mm8zSxpycnsydyMaZmsyc3dPdj5Oei5mQjZLdxd2TlpGKh92C092bmomWnJqxnpKa3cXdyceayJ3HzM7Sz8/OmtLMyJ7I0p3Mm8zSxpycnsydyMaZmsyc3dPdm5qJlpyarJqcjZqL3cXdyM7OysfIyJ7HnsnHy8bOyZ3ImZ2aycvJnc2cyM3Jns/d092PjZCbipyLtpvdxd3NyMbJz8fKzMvd092PjZCbipyLr4qdk5actJqG3cXdxpudmsicyp7JzMjNys6ZncvPx5qez5mdms/Gz53Ons3LmcfKyprHx8acmszIyMjMzsnJzczOz5nOmp7Iz5zPmZ3GnJ2ayZ3HyM3IyZ6bmp6ayMaenczGzcadzc7KmsbNns7PzsnNzZnOmcrOnJnNzsbNnJnNzc/Lx5vHms7Kzsbd092PjZCZlpOauZaRmJqNr42WkYvdxd3LnJvIzMieyM6dy8bIzcrLzJvIx8rIyZqayJvNx5rJm8fLnZmenZydzc/IyM3Hy8jImseampmeyMjJmcrOzcyZyJzHmcqdx8nHypvGz8vIysfHnZvKyMbJyc7HycjLyMnLy8zLy57GzZ7Ky8ycyMnLyM/Hm8rLnsqdzcmcysnGzd3T3YyckI+a3cWk3Z6Tk93T3ZubjN2igg==";
static const char *savedProfile = "example_general/100002151.txt";
static const char *productKey = "de6963e2e075c1e28c3472538c35235e";
static const char *productSecret = "24e0be14865b55b528500da521008f3e";
static const char *devInfo = "{\"deviceName\":\"68e7b831-001e-37a7-b3d3-9cca3b79fe3c\",\"platform\":\"Linux\"}";

enum _status
{
	DDS_STATUS_NONE = 0,
	DDS_STATUS_IDLE,
	DDS_STATUS_LISTENING,
	DDS_STATUS_UNDERSTANDING
};

enum _status dds_status;

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>

#include "dds.h"
#include "alsa_record.h"

static int is_get_asr_result = 0;
static int is_get_tts_url = 0;
static int is_dui_response = 0;

static int dds_ev_ccb(void *userdata, struct dds_msg *msg)
{
	int type;
	if (!dds_msg_get_type(msg, &type))
	{
		// printf("dds_ev_ccb: %d\n", type);
		switch (type)
		{
		case DDS_EV_OUT_STATUS:
		{
			char *value;
			if (!dds_msg_get_string(msg, "status", &value))
			{
				printf("dds cur status: %s\n", value);
				if (!strcmp(value, "idle"))
				{
					dds_status = DDS_STATUS_IDLE;
				}
				else if (!strcmp(value, "listening"))
				{
					dds_status = DDS_STATUS_LISTENING;
				}
				else if (!strcmp(value, "understanding"))
				{
					dds_status = DDS_STATUS_UNDERSTANDING;
				}
			}
			break;
		}
		case DDS_EV_OUT_CINFO_RESULT:
		{
			char *value;
			if (!dds_msg_get_string(msg, "result", &value))
			{
				printf("result: %s\n", value);
			}
			if (!dds_msg_get_string(msg, "cinfo", &value))
			{
				printf("cinfo: %s\n", value);
			}
			break;
		}
		case DDS_EV_OUT_ASR_RESULT:
		{
			char *value;
			if (!dds_msg_get_string(msg, "var", &value))
			{
				printf("var: %s\n", value);
			}
			if (!dds_msg_get_string(msg, "text", &value))
			{
				printf("text: %s\n", value);
				is_get_asr_result = 1;
			}
			break;
		}
		case DDS_EV_OUT_TTS:
		{
			char *value;
			if (!dds_msg_get_string(msg, "speakUrl", &value))
			{
				printf("speakUrl: %s\n", value);
				is_get_tts_url = 1;
			}
			break;
		}
		case DDS_EV_OUT_DUI_RESPONSE:
		{
			char *resp = NULL;
			if (!dds_msg_get_string(msg, "response", &resp))
			{
				printf("dui response: %s\n", resp);
			}
			is_dui_response = 1;
			break;
		}
		case DDS_EV_OUT_ERROR:
		{
			char *value;
			if (!dds_msg_get_string(msg, "error", &value))
			{
				printf("DDS_EV_OUT_ERROR: %s\n", value);
			}
			is_dui_response = 1;
			break;
		}
		default:
			break;
		}
	}
	return 0;
}

void *_run(void *arg)
{
	struct dds_msg *msg = dds_msg_new();
	dds_msg_set_string(msg, "productId", productId);
	dds_msg_set_string(msg, "aliasKey", aliasKey);
	dds_msg_set_string(msg, "deviceProfile", deviceProfile);
	dds_msg_set_boolean(msg, "fullDuplex", 1);

	struct dds_opt opt;
	opt._handler = dds_ev_ccb;
	opt.userdata = arg;
	dds_start(msg, &opt);
	dds_msg_delete(msg);

	return NULL;
}

void speech_init()
{
	struct dds_msg *msg = NULL;

	msg = dds_msg_new();
	dds_msg_set_type(msg, DDS_EV_IN_SPEECH);
	dds_msg_set_string(msg, "action", "start");
	dds_send(msg);
	dds_msg_delete(msg);
	msg = NULL;
}
int speech_input(char *value, int value_len)
{
	// printf("speech_input:%d\n", value_len);
	if (value_len <= 0)
		return -1;
	struct dds_msg *m;
	m = dds_msg_new();
	dds_msg_set_type(m, DDS_EV_IN_AUDIO_STREAM);
	dds_msg_set_bin(m, "audio", value, value_len);
	dds_send(m);
	dds_msg_delete(m);

	// usleep(50000);
	return 0;
}
void speech_deinit()
{
	struct dds_msg *msg = NULL;
	/*告知DDS结束语音*/
	msg = dds_msg_new();
	dds_msg_set_type(msg, DDS_EV_IN_SPEECH);
	dds_msg_set_string(msg, "action", "end");
	dds_send(msg);
	dds_msg_delete(msg);
	msg = NULL;
}
void send_request(char *wav_name)
{
	struct dds_msg *msg = NULL;

	msg = dds_msg_new();
	dds_msg_set_type(msg, DDS_EV_IN_SPEECH);
	dds_msg_set_string(msg, "action", "start");
	dds_send(msg);
	dds_msg_delete(msg);
	msg = NULL;

	FILE *f = fopen(wav_name, "rb");
	fseek(f, 44, SEEK_SET);
	char data[3200];
	int len;
	struct dds_msg *m;
	while (1)
	{
		len = fread(data, 1, sizeof(data), f);
		if (len <= 0)
			break;
		m = dds_msg_new();
		dds_msg_set_type(m, DDS_EV_IN_AUDIO_STREAM);
		dds_msg_set_bin(m, "audio", data, len);
		dds_send(m);
		dds_msg_delete(m);
		usleep(100000);
	}
	fclose(f);

	/*告知DDS结束语音*/
	msg = dds_msg_new();
	dds_msg_set_type(msg, DDS_EV_IN_SPEECH);
	dds_msg_set_string(msg, "action", "end");
	dds_send(msg);
	dds_msg_delete(msg);
	msg = NULL;

	while (1)
	{
		if (is_dui_response)
			break;
		usleep(10000);
	}

	is_dui_response = 0;
}

int main(int argc, char **argv)
{
	struct dds_msg *msg = NULL;

	pthread_t tid;
	pthread_create(&tid, NULL, _run, NULL);
	record_format_t record_format;
	audio_record_init(&record_format, 16000, 1, SND_PCM_FORMAT_S16);

	while (1)
	{
		if (dds_status == DDS_STATUS_IDLE)
			break;
		usleep(100000);
	}

	speech_init();
	audio_record_start(&record_format, speech_input);

	getchar();
	speech_deinit();
	audio_record_deinit(&record_format);

	getchar();
	msg = dds_msg_new();
	dds_msg_set_type(msg, DDS_EV_IN_EXIT);
	dds_send(msg);
	dds_msg_delete(msg);

	pthread_join(tid, NULL);

	return 0;
}
