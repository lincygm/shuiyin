// FFMPEG_TEST.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <stdio.h>
extern "C"
{
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libswscale/swscale.h"
#include "sdl/SDL.h"
#include "sdl/SDL_thread.h"
#include "libavfilter/avfiltergraph.h"  
#include "libavfilter/buffersink.h"  
#include "libavfilter/buffersrc.h" 

}
using namespace std;

#define SHOW_FULLSCREEN 0
#define OUTPUT_YUV420P 1

	AVFilterGraph *filter_graph;
	AVFilterContext *buffersink_ctx;
	AVFilterContext *buffersrc_ctx;
	AVCodecContext *pCodecCtx;
	const char *filter_descr = "movie=yk.png[wm];[in][wm]overlay=1:1[out]";

static int init_filters(const char *filters_descr)
{
	char args[512];
	int ret;
	AVFilter *buffersrc = avfilter_get_by_name("buffer");
	AVFilter *buffersink = avfilter_get_by_name("ffbuffersink");
	AVFilterInOut *outputs = avfilter_inout_alloc();
	AVFilterInOut *inputs = avfilter_inout_alloc();
	enum AVPixelFormat pix_fmts[] = { AV_PIX_FMT_YUV420P, AV_PIX_FMT_NONE };
	AVBufferSinkParams *buffersink_params;

	filter_graph = avfilter_graph_alloc();

	/* buffer video source: the decoded frames from the decoder will be inserted here. */
	_snprintf(args, sizeof(args),
		"video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d",
		pCodecCtx->width, pCodecCtx->height, pCodecCtx->pix_fmt,
		pCodecCtx->time_base.num, pCodecCtx->time_base.den,
		pCodecCtx->sample_aspect_ratio.num, pCodecCtx->sample_aspect_ratio.den);

	ret = avfilter_graph_create_filter(&buffersrc_ctx, buffersrc, "in",
		args, NULL, filter_graph);
	if (ret < 0) {
		printf("Cannot create buffer source\n");
		return ret;
	}

	/* buffer video sink: to terminate the filter chain. */
	buffersink_params = av_buffersink_params_alloc();
	buffersink_params->pixel_fmts = pix_fmts;
	ret = avfilter_graph_create_filter(&buffersink_ctx, buffersink, "out",
		NULL, buffersink_params, filter_graph);
	av_free(buffersink_params);
	if (ret < 0) {
		printf("Cannot create buffer sink\n");
		return ret;
	}

	/* Endpoints for the filter graph. */
	outputs->name = av_strdup("in");
	outputs->filter_ctx = buffersrc_ctx;
	outputs->pad_idx = 0;
	outputs->next = NULL;

	inputs->name = av_strdup("out");
	inputs->filter_ctx = buffersink_ctx;
	inputs->pad_idx = 0;
	inputs->next = NULL;

	if ((ret = avfilter_graph_parse_ptr(filter_graph, filters_descr,
		&inputs, &outputs, NULL)) < 0)
		return ret;

	if ((ret = avfilter_graph_config(filter_graph, NULL)) < 0)
		return ret;
	return 0;
}


int _tmain(int argc, _TCHAR* argv[])
{

	AVFormatContext *pFormatCtx;
	int i, videoindex;
	AVCodec *pCodec;
	char filePath[] = "cuc_ieschool.flv.";
	av_register_all();
	avfilter_register_all();
	avformat_network_init();
	pFormatCtx = avformat_alloc_context();
	if (avformat_open_input(&pFormatCtx, filePath, NULL, NULL)){
		printf("could not open input file stream");
		return -1;
	}

	if (av_find_stream_info(pFormatCtx) < 0){
		return -1;
	}
	videoindex = -1;
	for (i = 0; i < pFormatCtx->nb_streams; i++)
	{
		if (pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO){
			videoindex = i;
			break;
		}
	}
	if (videoindex == -1){
		return -1;
	}
	pCodecCtx = pFormatCtx->streams[videoindex]->codec;
	pCodec = avcodec_find_decoder(pCodecCtx->codec_id);
	if (pCodec == NULL){
		return -1;
	}
	if (avcodec_open2(pCodecCtx, pCodec, NULL) < 0){
		return -1;
	}

	AVFrame *pFrame, *pFrameYUV;
	pFrame = avcodec_alloc_frame();
	pFrameYUV = avcodec_alloc_frame();
	uint8_t  *out_buffer = (uint8_t *)av_malloc(avpicture_get_size(PIX_FMT_YUV420P, pCodecCtx->width, pCodecCtx->height));

	avpicture_fill((AVPicture *)pFrameYUV, out_buffer, PIX_FMT_YUV420P, pCodecCtx->width, pCodecCtx->height);

	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER)){
		return -1;
	}
	init_filters(filter_descr);

#if OUTPUT_YUV420P
	FILE *fp_yuv = fopen("output.yuv", "wb+");
	printf("dsdasdasdasdas  \n");
#endif

	int screen_w = 0, screen_h = 0;
	SDL_Surface *screen;
#if SHOW_FULLSCREEN
	const SDL_VideoInfo *vi = SDL_GetVideoInfo();
	screen_h = vi->current_h;
	screen_w = vi->current_w;
	screen = SDL_SetVideoMode(screen_w,screen_h,0,SDL_FULLSCREEN);
#else
	screen_w = pCodecCtx->width;
	screen_h = pCodecCtx->height;
	screen = SDL_SetVideoMode(screen_w, screen_h
		, 0, 0);
#endif
	if (!screen){
		return -1;
	}
	SDL_Overlay *bmp;
	bmp = SDL_CreateYUVOverlay(pCodecCtx->width, pCodecCtx->height, SDL_YV12_OVERLAY, screen);
	SDL_Rect rect;
	int ret, got_picture;

	AVPacket *packet = (AVPacket *)av_malloc(sizeof(AVPacket));
	av_dump_format(pFormatCtx, 0, filePath, 0);
	int got_frame;


	struct SwsContext   *img_convert_ctx;
	img_convert_ctx = sws_getContext(pCodecCtx->width,
		pCodecCtx->height, pCodecCtx->pix_fmt, pCodecCtx->width, pCodecCtx->height, PIX_FMT_YUV420P, SWS_BICUBIC, NULL, NULL, NULL);
	while (av_read_frame(pFormatCtx, packet) >= 0){

		if (packet->stream_index == videoindex){
			got_frame = 0;
			ret = avcodec_decode_video2(pCodecCtx, pFrame, &got_picture, packet);
			if (ret < 0){
				return -1;
			}
			if (got_picture){

				sws_scale(img_convert_ctx, (const uint8_t* const*)pFrame->data, pFrame->linesize, 0, pCodecCtx->height, pFrameYUV->data, pFrameYUV->linesize);

				//===========================加水印==========================
				pFrame->pts = av_frame_get_best_effort_timestamp(pFrame);
				/* push the decoded frame into the filtergraph */
				if (av_buffersrc_add_frame(buffersrc_ctx, pFrame) < 0) {
					printf("Error while feeding the filtergraph\n");
					break;
				}
				av_buffersink_get_frame(buffersink_ctx, pFrameYUV);
				//=======================加水印==============================

#if OUTPUT_YUV420P
				int y_size=pCodecCtx->width*pCodecCtx->height;

				fwrite(pFrameYUV->data[0],1,y_size,fp_yuv);
				fwrite(pFrameYUV->data[1], 1, y_size / 4, fp_yuv);  //U
				fwrite(pFrameYUV->data[2], 1, y_size / 4, fp_yuv);  //V
#endif
				SDL_LockYUVOverlay(bmp);
				memcpy(bmp->pixels[0], pFrameYUV->data[0], y_size);   //Y  
				memcpy(bmp->pixels[2], pFrameYUV->data[1], y_size / 4); //U  
				memcpy(bmp->pixels[1], pFrameYUV->data[2], y_size / 4); //V   
				bmp->pixels[0] = pFrameYUV->data[0];
				bmp->pixels[2] = pFrameYUV->data[1];
				bmp->pixels[1] = pFrameYUV->data[2];
				bmp->pitches[0] = pFrameYUV->linesize[0];
				bmp->pitches[2] = pFrameYUV->linesize[1];
				bmp->pitches[1] = pFrameYUV->linesize[2];
				SDL_UnlockYUVOverlay(bmp);
				rect.x = 0;
				rect.y = 0;
				rect.w = screen_w;
				rect.h = screen_h;
				SDL_DisplayYUVOverlay(bmp, &rect);
				//Delay 40ms
				SDL_Delay(40);
			}
			}
		
		av_free_packet(packet);
	}
	sws_freeContext(img_convert_ctx);
#if OUTPUT_YUV420P 
	fclose(fp_yuv);
#endif 

	SDL_Quit();

	av_free(out_buffer);
	av_free(pFrameYUV);
	avcodec_close(pCodecCtx);
	avformat_close_input(&pFormatCtx);

	/*int n = 2;
	int *pn = &n;
	printf("%d \n", *pn);
	func(&pn);
	printf("%d \n", *pn);
	system("pause");*/
	return 0;
}