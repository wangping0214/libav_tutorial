extern "C"
{
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
}
#include <stdio.h>
#include <Windows.h>

void SaveFrame(AVFrame *pFrame, int width, int height, int iFrame) 
{
	FILE *pFile;
	char szFilename[32];
  
	// Open file
	sprintf(szFilename, "frame%d.bmp", iFrame);
	pFile = fopen(szFilename, "wb");
	if(pFile == NULL)
		return;
  
	int wBitCount = 24;	// 表示由3个字节定义一个像素
	int bmByteCount =  pFrame->linesize[0] * height;		// 像素数据的大小
	// 位图信息头结构，定义参考MSDN
	BITMAPINFOHEADER bi;
	bi.biSize = sizeof(BITMAPINFOHEADER);
	bi.biWidth = width;
	bi.biHeight = height;
	bi.biPlanes = 1;
	bi.biBitCount = wBitCount;
	bi.biCompression= BI_RGB;         
	bi.biSizeImage=0;         
	bi.biXPelsPerMeter = 0;         
	bi.biYPelsPerMeter = 0;         
	bi.biClrImportant = 0;         
	bi.biClrUsed =  0;
	// 位图文件头结构，定义参考MSDN
	BITMAPFILEHEADER bf;  
	bf.bfType = 0x4D42; // BM  
	bf.bfSize = bmByteCount;  
	bf.bfReserved1 = 0;  
	bf.bfReserved2 = 0;  
	bf.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);

	fwrite(&bf, sizeof(BITMAPFILEHEADER), 1, pFile);
	fwrite(&bi, sizeof(BITMAPINFOHEADER), 1, pFile);
	for (int i = 0; i < height; ++ i)
		fwrite(pFrame->data[0] + (height - i - 1)*pFrame->linesize[0], 1, pFrame->linesize[0], pFile);

	// Close file
	fclose(pFile);
}

int main(int argc, char *argv[]) 
{
	AVFormatContext *pFormatCtx = NULL;
	int             i, videoStream;
	AVCodecContext  *pCodecCtx;
	AVCodec         *pCodec;
	AVFrame         *pFrame; 
	AVFrame         *pFrameRGB;
	AVPacket        packet;
	SwsContext		*swsContextPtr = NULL;
	int             frameFinished;
	int             numBytes;
	uint8_t         *buffer;
  
	if (argc < 2) 
	{
		printf("Please provide a movie file\n");
		return -1;
	}
	// Register all formats and codecs
	av_register_all();
  
	printf(argv[1]);
	// Open video file
	if (avformat_open_input(&pFormatCtx, argv[1], NULL, NULL) != 0)
		return -1; // Couldn't open file
  
	// Retrieve stream information
	if (avformat_find_stream_info(pFormatCtx, NULL) < 0)
		return -1; // Couldn't find stream information
  
	// Dump information about file onto standard error
	av_dump_format(pFormatCtx, 0, argv[1], 0);
  
	// Find the first video stream
	videoStream = -1;
	for (i = 0; i < pFormatCtx->nb_streams; i ++)
	{
		if (pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO)
		{
			videoStream=i;
			break;
		}
	}
	if (videoStream == -1)
		return -1; // Didn't find a video stream
  
	// Get a pointer to the codec context for the video stream
	pCodecCtx = pFormatCtx->streams[videoStream]->codec;
  
	// Find the decoder for the video stream
	pCodec = avcodec_find_decoder(pCodecCtx->codec_id);
	if (pCodec == NULL)
	{
		fprintf(stderr, "Unsupported codec!\n");
			return -1; // Codec not found
	}
	// Open codec
	if (avcodec_open2(pCodecCtx, pCodec, NULL) < 0)
		return -1; // Could not open codec
  
	// Allocate video frame
	pFrame = avcodec_alloc_frame();
  
	// Allocate an AVFrame structure
	pFrameRGB = avcodec_alloc_frame();
	if (pFrameRGB == NULL)
		return -1;
  
	// Determine required buffer size and allocate buffer
	numBytes = avpicture_get_size(PIX_FMT_BGR24, pCodecCtx->width, pCodecCtx->height);
	buffer = (uint8_t*)av_malloc_array(numBytes, sizeof(uint8_t));
  
	// Assign appropriate parts of buffer to image planes in pFrameRGB
	// Note that pFrameRGB is an AVFrame, but AVFrame is a superset
	// of AVPicture
	avpicture_fill((AVPicture *)pFrameRGB, buffer, PIX_FMT_BGR24, pCodecCtx->width, pCodecCtx->height);
  
	// Read frames and save first five frames to disk
	i = 0;
	while (av_read_frame(pFormatCtx, &packet) >= 0)
	{
		// Is this a packet from the video stream?
		if(packet.stream_index == videoStream) 
		{
			// Decode video frame
			avcodec_decode_video2(pCodecCtx, pFrame, &frameFinished, &packet);
      
			// Did we get a video frame?
			if(frameFinished)
			{
				// Convert the image from its native format to RGB
				swsContextPtr = sws_getCachedContext(swsContextPtr, pCodecCtx->width, pCodecCtx->height, pCodecCtx->pix_fmt, 
					pCodecCtx->width, pCodecCtx->height, PIX_FMT_BGR24, SWS_BICUBIC, NULL, NULL, NULL);
				if (NULL == swsContextPtr)
				{
					printf("Could not initialize the conversion context.\n");
					return -1;
				}
				sws_scale(swsContextPtr, pFrame->data, pFrame->linesize, 0, pCodecCtx->height, pFrameRGB->data, pFrameRGB->linesize);
	
				// Save the frame to disk
				if(++i <= 5)
					SaveFrame(pFrameRGB, pCodecCtx->width, pCodecCtx->height, i);
			}
		}
    
		// Free the packet that was allocated by av_read_frame
		av_free_packet(&packet);
	}
  
	// Free the RGB image
	av_free(buffer);
	av_free(pFrameRGB);
  
	// Free the YUV frame
	av_free(pFrame);
  
	// Close the codec
	avcodec_close(pCodecCtx);
  
	// Close the video file
	avformat_close_input(&pFormatCtx);
  
	return 0;
}
