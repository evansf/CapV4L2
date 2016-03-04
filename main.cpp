// Main
// This uses CameraV4L2 object to capture a wierd format from
// the See3CAM_CU40 which is a USB camera that captures RGB
// and IR images simultaneously.  It has a Bayer pattern that
// is  B G    instead of  B G
//    IR R                G R
// It only supports a Y16 format of the Raw Bayer pattern data
// so it is nearly impossible to utilize standard streaming formats
// to get sensible images.  The CameraV4L2 class is used to control
// the camera and get buffers of the raw data.  This ExtractBayer10_Y16() 
// routine takes the raw data and creates RGB and IR OpenCV Mat
// objects from the data.  The capture routine gets the buffers from the Y16
// stream on the camera and places the images into OpenCV named windows
//  (as a "viewFinder").  When [anykey] is pressed capture returns to main()
// where the images are written to files as ./RGB.jpg and ./IR.jpg

#include "camerav4l2.h"
#include <opencv2/imgproc/imgproc.hpp>

// ***********************************************************************
// ******** Routine to turn buffers of Bayer data into cv::Mats **********
// ***********************************************************************
//Macros to assist in Bayer Conversion
#define B(x,y,w) pDstRGB[0 + 3 * ((x) + (w) * (y))] 
#define G(x,y,w) pDstRGB[1 + 3 * ((x) + (w) * (y))] 
#define R(x,y,w) pDstRGB[2 + 3 * ((x) + (w) * (y))] 
#define IR(x,y,w) pDstIR[0 +     ((x) + (w) * (y))] 
#define BAY(x,y,w) pSrc[(x) + (w) * (y)]
#define CLIP(x) ((x) < 0 ? 0 : ((x) >= 255 ? 255 : (x)))
#define CLIP10(x) ((x) < 0 ? 0 : ((x) >= 1023 ? 1023 : (x)))
float IRGain[3] = {1.0f, 1.0f, 1.0f};

// Extract 10 bit data from Y16 to 8 bit data RGB8 and IR8
// No gain is applied and [0..255] of the [0..1023] range is all that is used
static cv::Point2i ExtractBayerY16toRGB8(cv::Mat &dstRGB,cv::Mat &dstIR, uint8_t * src, int srcLen, cv::Point2i start)
{
	cv::Point2i last;
	unsigned char *pDstRGB;
	unsigned char *pDstIR;
	unsigned short *pSrc = (unsigned short*) src;
	int x,y;
	int srccnt = 0;	int width = dstRGB.cols;  int height = dstRGB.rows;
	// itterate 2X2 to de-Bayer and spread out R,G,B elements to RGB and put IR to IR
	// subtract the IR signal from all other sensor colors
	unsigned short IRVal;
	for (y = start.y; (y < height) ; y+=2)
	{
		if( srccnt >= srcLen )
			break;
		pDstRGB = dstRGB.ptr(y);
		pDstIR  = dstIR.ptr(y);
		for (x = start.x; (x < width) ; x+=2)
		{
			IRVal = CLIP(BAY(x  ,y+1,width));
			IR(x,0,width) = IR(x+1,0, width) = IR(x,1,width) = IR(x+1,1,width) = CLIP(IRVal);
			B(x,0,width)   = B(x+1,0, width) =  B(x,1,width) =  B(x+1,1,width) = CLIP(BAY(x  ,y  ,width) - (int)(IRGain[0] * IRVal));
			G(x,0,width)   = G(x+1,0, width) =  G(x,1,width) =  G(x+1,1,width) = CLIP(BAY(x+1,y  ,width) - (int)(IRGain[1] * IRVal));
			R(x,0,width)   = R(x+1,0, width) =  R(x,1,width) =  R(x+1,1,width) = CLIP(BAY(x+1,y+1,width) - (int)(IRGain[2] * IRVal));
			srccnt+=8;	// used 4 bytes from two rows
		}
	}
	last.x = x; last.y = y;
	return last;
}
// Extract 10 bit data from Y16 to 10 bit data RGB16 and IR16
// No gain is applied and [0..1023] of the [0..1023] range is all used
static cv::Point2i ExtractBayerY16toRGB16(cv::Mat &dstRGB,cv::Mat &dstIR, uint8_t * src, int srcLen, cv::Point2i start)
{
	cv::Point2i last;
	unsigned short *pDstRGB;
	unsigned short *pDstIR;
	unsigned short *pSrc = (unsigned short*) src;
	int x,y;
	int srccnt = 0;
	int width = dstRGB.cols;  int height = dstRGB.rows;
	short IRVal;
	unsigned char * pBuf, *pDst;
	// itterate 2X2 to de-Bayer and spread out R,G,B elements to RGB and put IR to IR
	// elements are in Pattern  B G
	//                         IR R
	// subtract the IR signal from all other sensor colors
	// To prevent overflow,  only use 9 bits of raw data 
	for (y = start.y; (y < height) ; y+=2)
	{
		if( srccnt >= srcLen )
			break;
		pDstRGB = (unsigned short*)dstRGB.ptr(y);
		pDstIR  = (unsigned short*)dstIR.ptr(y);
		for (x = start.x; (x < width) ; x+=2)
		{
			IRVal = BAY(x  ,y+1,width);
			B(x,0,width)   = B(x+1,0, width) =  B(x,1,width) =  B(x+1,1,width) = CLIP10(2*(BAY(x  ,y  ,width) - (int)(IRGain[0] * IRVal)));
			G(x,0,width)   = G(x+1,0, width) =  G(x,1,width) =  G(x+1,1,width) = CLIP10(2*(BAY(x+1,y  ,width) - (int)(IRGain[1] * IRVal)));
			R(x,0,width)   = R(x+1,0, width) =  R(x,1,width) =  R(x+1,1,width) = CLIP10(2*(BAY(x+1,y+1,width) - (int)(IRGain[2] * IRVal)));
			IR(x,0,width) = IR(x+1,0, width) = IR(x,1,width) = IR(x+1,1,width) = CLIP10(2*IRVal);
			srccnt+=8;	// used 4 bytes from two rows
		}
	}
	last.x = x; last.y = y;
	return last;
}
static cv::Point2i ExtractBayerY16toRGB(cv::Mat &dstRGB, cv::Mat &dstIR, uint8_t * src, int srcLen, cv::Point2i start)
{
	cv::Point2i p;
	int depth = dstRGB.depth();
	switch (depth)
	{
	case 0:
		p = ExtractBayerY16toRGB8(dstRGB,dstIR, src, srcLen, start);
		break;
	case 2:
		p = ExtractBayerY16toRGB16(dstRGB,dstIR, src, srcLen, start);
		break;
	default:
		break;
	}
	return p;
}

// ********************************************************************************
// ****  Routine to capture data from buffers into OpenCV Mat objects  ************
// ********************************************************************************
// CaptureImage() accepts a pointer to a CameraV4L2 object and references to two
// cv::Mat images which must be pre-created as follows:
//		cv::Mat RGB(height,width,CV_8UC3);
//		cv::Mat  IR(height,width,CV_8UC1);
// It displays camera images continuously to OpenCV named windows
// when [anykey] is pressed, CaptureImage() it returns with the images in the cv::Mats
#define SQRT2 1.414213562F
#define SQRT2INV 0.707106781F
#define RGB16 1
static int CaptureImage(CameraV4L2 *pCap, cv::Mat &RGB, cv::Mat &IR, bool sRGB = true )
{
	int height = RGB.rows;
	int width = RGB.cols;
	cv::Point2i start = cv::Point2i(0,0);
	int bufLen; 
//	std::string videoName("/tmp/Viewfinder.avi");
	std::string videoName("/tmp/Viewfinder.avi/");
//	std::string videoName("http://localhost/feed1.ffm/");
	int fourcc = CV_FOURCC('M','J','P','G');
	int fps = 100;
	int viewWidth = width/2; int viewHeight = height/2;
	viewWidth += (viewWidth % 16);  viewHeight += (16-viewHeight % 16);
	cv::Size frameSize(viewWidth,viewHeight);
	cv::Mat ViewMat(viewHeight,viewWidth,RGB.type());
	bool isColor = true;
	cv::VideoWriter outputVideo;
	outputVideo.open(videoName, fourcc, fps, frameSize,isColor);

	bool status = outputVideo.isOpened();
	cv::Mat xRGB(RGB.size(),RGB.type());
	cv::Mat xIR(IR.size(),IR.type());
 	pCap->Start();
	int key = -1;
	while (key == -1)	// anykey to exit
	{
		do
		{
			bufLen = pCap->WaitForFrame();
			start = ExtractBayerY16toRGB(xRGB, xIR, pCap->Buffer(), bufLen, start);
		} while (start.y < height);
		start = cv::Point2i(0,0);	// restart capture for next loop
		
		if (sRGB)
		{
			pCap->ConvertTosRGB(xRGB,RGB);
			pCap->ConvertTosRGB(xIR,IR);
		}
		std::vector<cv::Mat> plane;
		cv::split(RGB,plane);
		cv::imshow("frameB",plane[0]);
		cv::imshow("frameG",plane[1]);
		cv::imshow("frameR",plane[2]);
		cv::imshow("frameRGB",RGB);	// viewfinder displays
		cv::imshow("frameIR",IR);
		
		cv::resize(RGB,ViewMat,ViewMat.size(),0,0, CV_INTER_NN);
		outputVideo << ViewMat;
		key = cv::waitKey(20);	// catch key
		if(key == -1) continue;
		int current;
		switch (key)
		{
		case 1113938:
			// increase exposure by 1/2 stop
			current = pCap->GetExposure();
			if (current >= 0)
			{
				current = (int)((float)current * SQRT2);
				current = (current < 3 ) ? ++current : current;
				pCap->SetExposure(current);
			}
			key = -1;
			break;
		case 1113940:
			// decrease exposure by half stop
			current = pCap->GetExposure();
			if(current >= 0)
			{
				current = (int)((float)current * SQRT2INV);
				pCap->SetExposure(current);
			}
			key = -1;
			break;
		default:
			break;
		}
	}	// while(key)
	
	outputVideo.release();
	pCap->Stop();
    return 0;
}

// *************************************************************************
// **************  Main For Testing  ***************************************
// *************************************************************************
// This main tests See3CAM_CU40 which is RGB-IR camera which only outputs
// Y16 formatted data of its Bayer pixels.  It provides 10 bit data.
// This format is nearly impossible to support using standard streams.
int main(int argc, char* argv[])
{
	int width = 672; int height = 380;
	if (argc > 1)
	{
		char* argstring0 = argv[0];
		char* argstring1 = argv[1];
		sscanf(argv[1], "%dx%d", &width, &height);
	}
	CameraV4L2 cam("/dev/video0",512);
	if(!cam.Exists())
	{
		fprintf(stderr, "Unable to Open Camera");
		return 0;
	}
	if (cam.SetSize(width, height))
	{
		fprintf(stderr, "Requested Size %dx%d is not supported",width,height);
		exit -1;
	}
    if(cam.PrintCaps())	// also sets up m_fmt
        return 1;
	if(cam.RequestBuffers(1))
		return 1;
	
	cv::Mat frameRGB;
	cv::Mat frameIR;
	if (RGB16)
	{
		frameRGB.create(height, width, CV_16UC3);
		frameIR.create(height, width, CV_16UC1);
	}
	else
	{
		frameRGB.create(height, width, CV_8UC3);
		frameIR.create(height, width, CV_8UC1);
	}
    if(CaptureImage(&cam, frameRGB, frameIR))
        return 1;
	
	printf ("saving images\n");
	cv::imwrite("/home/frank/Pictures/RGB.png",frameRGB);
	cv::imwrite("/home/frank/Pictures/IR.png",frameIR);
	
    return 0;
}