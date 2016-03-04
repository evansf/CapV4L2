#ifndef CAMERAV4L2_HEADER
#define CAMERAV4L2_HEADER
#include <errno.h>
#include <fcntl.h>
#include <linux/videodev2.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>

class CameraV4L2
{
public:
	typedef enum errors
	{
		OK = 0,
		FAIL
	} ERR;
	
	CameraV4L2(std::string device, int inputRange = 1024);
	~CameraV4L2();
	bool Exists();
	ERR SetSize(int width, int height);	// FAIL if not supported
	ERR PrintCaps();
	ERR	SetFormat(struct v4l2_format &fmt);
	ERR	GetFormat(struct v4l2_format &fmt);
	ERR RequestBuffers(int n=1);
	int WaitForFrame();	// returns number of bytes in buffer or -1
	ERR Start();
	ERR Stop();
	uint8_t* Buffer(){return m_Buffer;};
	int GetExposure();	// in 1/10 ms
	ERR SetExposure(int tenth_ms);
	int GetBrightness();	// [0..40]
	ERR SetBrightness(int val);
	ERR ConvertTosRGB(cv::Mat &src, cv::Mat &dst);
	
protected:
	// this device
	int m_fd;
	struct v4l2_capability m_Caps;
	int m_Width, m_Height;
    struct v4l2_format m_Fmt;
	ERR	QueryBuffer(struct v4l2_buffer &buf);
	ERR EnQueBuffer(struct v4l2_buffer &buf);
	ERR DeQueBuffer(struct v4l2_buffer &buf);
	void showflags(int flags);
	
	int xioctl(int request, void *arg);
	
private:
	std::string m_DeviceName;
	// mapped pointer to current buffer
	struct v4l2_buffer m_buf;	// current buffer in play
	uint8_t *m_Buffer;	// mapped location of m_buf
	static unsigned char sRGBVal8[1024];
	static unsigned short sRGBVal16[1024];
};

#endif // CAMERAV4L2_HEADER