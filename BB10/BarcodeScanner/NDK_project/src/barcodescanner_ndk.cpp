/*
 * Copyright 2013 Research In Motion Limited.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <json/reader.h>
#include <json/writer.h>

#include <zxing/common/GreyscaleLuminanceSource.h>
#include <zxing/common/HybridBinarizer.h>
#include <zxing/qrcode/QRCodeReader.h>
#include <zxing/MultiFormatReader.h>
#include <img/img.h>
#include <stdio.h>

#include "../public/base64/base64.h"

#include "barcodescanner_ndk.hpp"
#include "barcodescanner_js.hpp"

#include <sstream>

using namespace zxing;
using namespace zxing::qrcode;

namespace webworks {

BarcodeScannerJS* eventDispatcher = NULL;
static int filecounter = 0;
#define TMP_PATH "tmp/"
static uint32_t rotation = 0;
static img_lib_t ilib = NULL;

    /*
     * getCameraErrorDesc
     *
     * Returns a descriptive error message for a given camera error code
     */
    const char* getCameraErrorDesc(camera_error_t err) {
        char* ret;
        switch (err) {
        case CAMERA_EAGAIN:
            return "The specified camera was not available. Try again.";
        case CAMERA_EINVAL:
            return "The camera call failed because of an invalid parameter.";
        case CAMERA_ENODEV:
            return "No such camera was found.";
        case CAMERA_EMFILE:
            return "The camera called failed because of a file table overflow.";
        case CAMERA_EBADF:
            return "Indicates that an invalid handle to a @c camera_handle_t value was used.";
        case CAMERA_EACCESS:
            return "Indicates that the necessary permissions to access the camera are not available.";
        case CAMERA_EBADR:
            return "Indicates that an invalid file descriptor was used.";
        case CAMERA_ENOENT:
            return "Indicates that the access a file or directory that does not exist.";
        case CAMERA_ENOMEM:
            return "Indicates that memory allocation failed.";
        case CAMERA_EOPNOTSUPP:
            return "Indicates that the requested operation is not supported.";
        case CAMERA_ETIMEDOUT:
            return "Indicates an operation on the camera is already in progress. In addition, this error can indicate that an error could not be completed because i was already completed. For example, if you called the @c camera_stop_video() function but the camera had already stopped recording video, this error code would be returned.";
        case CAMERA_EALREADY:
            return "Indicates an operation on the camera is already in progress. In addition,this error can indicate that an error could not be completed because it was already completed. For example, if you called the @c camera_stop_video() function but the camera had already stopped recording video, this error code would be returned.";
        case CAMERA_EUNINIT:
            return "Indicates that the Camera Library is not initialized.";
        case CAMERA_EREGFAULT:
            return "Indicates that registration of a callback failed.";
        case CAMERA_EMICINUSE:
            return "Indicates that it failed to open because microphone is already in use.";
        }
        return NULL;
    }


    /*
     * viewfinder_callback
     *
     * This callback is invoked when an image frame from the camera viewfinder becomes available.
     * The frame is analyzed to determine if a barcode can be matched.
     */
    void viewfinder_callback(camera_handle_t handle,camera_buffer_t* buf,void* arg) {
        camera_frame_nv12_t* data = (camera_frame_nv12_t*)(&(buf->framedesc));
        uint8_t* buff = buf->framebuf;
        int stride = data->stride;
        int width = data->width;
        int height = data->height;

//        std::string rgbFrame = YUV_NV12_TO_RGB((unsigned char *)buff, width, height);
//        std::string bitmapHeader = getBitmapHeader(width, height);
//
//        Json::FastWriter writer;
//        Json::Value root;
//        root["header"] = base64_encode((const unsigned char *)bitmapHeader.c_str(), bitmapHeader.length());
//        root["frame"]  = base64_encode((const unsigned char *)rgbFrame.c_str(), rgbFrame.length());;
//        std::string event = "community.QRCodeReader.cameraFrameCallback.native";
//
//        // push encoded frame back to caller
//        QRCodeReaderNDK::m_pParent->NotifyEvent(event + " " + writer.write(root));

        try {
            Ref<LuminanceSource> source(new GreyscaleLuminanceSource((unsigned char *)buff, stride, height, 0,0,width,height));

            Ref<Binarizer> binarizer(new HybridBinarizer(source));
            Ref<BinaryBitmap> bitmap(new BinaryBitmap(binarizer));
            Ref<Result> result;

            // setup the QR code reader
            QRCodeReader *reader = new QRCodeReader();
            DecodeHints *hints = new DecodeHints();

            hints->addFormat(BarcodeFormat_QR_CODE);
            hints->addFormat(BarcodeFormat_EAN_8);
            hints->addFormat(BarcodeFormat_EAN_13);
            hints->addFormat(BarcodeFormat_UPC_A);
            hints->addFormat(BarcodeFormat_UPC_E);
            hints->addFormat(BarcodeFormat_DATA_MATRIX);
            hints->addFormat(BarcodeFormat_CODE_128);
            hints->addFormat(BarcodeFormat_CODE_39);
            hints->addFormat(BarcodeFormat_ITF);
            hints->addFormat(BarcodeFormat_AZTEC);

            // attempt to decode and retrieve a valid QR code from the image bitmap
            result = reader->decode(bitmap, *hints);
            std::string newBarcodeData = result->getText()->getText().data();

            Json::FastWriter writer;
            Json::Value root;
            root["value"] = newBarcodeData;
            std::string event = "community.barcodescanner.codefound.native";

            // notify caller that a valid QR code has been decoded
            if ( eventDispatcher != NULL ){
            	 eventDispatcher->NotifyEvent(event + " " + writer.write(root));
            }


#ifdef DEBUG
            fprintf(stderr, "This is the detected Barcode : %s\n", newBarcodeData.c_str());
#endif
        }
        catch (const std::exception& ex)
        {
#ifdef DEBUG
            fprintf( stderr, "error occured : \%s \n", ex.what());
#endif
        }
    }

    std::string convertIntToString(int i) {
		stringstream ss;
		ss << i;
		return ss.str();
	}

    /*
     * image_callback
     *
     * handles the burst frames from the camera
     */
    void image_callback(camera_handle_t handle,camera_buffer_t* buf,void* arg) {
       	camera_frame_jpeg_t* data = (camera_frame_jpeg_t*)(&(buf->framedesc));
       	uint8_t* buff = buf->framebuf;

       	if (buf->frametype == CAMERA_FRAMETYPE_JPEG) {
			fprintf(stderr, "still image size: %lld\n", buf->framedesc.jpeg.bufsize);

			Json::FastWriter writer;
			Json::Value root;

			std::string tempFileName = "barcode" + convertIntToString(filecounter) + ".jpg";
			if (++filecounter >=10) {
				filecounter = 0;
			}
			std::string tempFilePath = std::string(getcwd(NULL, 0)) + "/" + TMP_PATH + tempFileName;
			FILE* fp = fopen(tempFilePath.c_str(), "wb");
			if (fp!= NULL) {
				fwrite((const unsigned char *)buf->framebuf, buf->framedesc.jpeg.bufsize, 1, fp);
				fclose(fp);
			}

			img_t img;
			if (rotation == 0 || rotation == 2) {
				img.w = 720;
				img.flags = IMG_W;
			} else {
				img.h = 720;
				img.flags = IMG_H;
			}
			int resizeResult = img_load_resize_file( ilib, tempFilePath.c_str(), NULL, &img );
			img_t dst;
			img_fixed_t angle = 0;
			switch (rotation) {
			case 1:
				angle = IMG_ANGLE_90CCW;
				break;
			case 2:
				angle = IMG_ANGLE_180;
				break;
			case 3:
				angle = IMG_ANGLE_90CW;
				break;
			default:
				break;
			}
			if (angle != 0) {
				int err = img_rotate_ortho(&img, &dst, angle);
			} else {
				dst = img;
			}
			int writeResult = img_write_file( ilib, tempFilePath.c_str(), NULL, &dst );

			root["frame"]  = tempFilePath;
			root["orientation"] = buf->frameorientation;
			root["frametype"] = buf->frametype;
			root["rotation"] = rotation;
			std::string event = "community.barcodescanner.frameavailable.native";
			if ( eventDispatcher != NULL ){
				 eventDispatcher->NotifyEvent(event + " " + writer.write(root));
			}
       	}

    }

    /*
     * Constructor for Barcode Scanner NDK class
     */
    BarcodeScannerNDK::BarcodeScannerNDK(BarcodeScannerJS *parent) {
        m_pParent     = parent;
        eventDispatcher = parent;
        mCameraHandle = CAMERA_HANDLE_INVALID;
    }

    BarcodeScannerNDK::~BarcodeScannerNDK() {}


    /*
     * BarcodeScannerNDK::startRead
     *
     * This method is called to start a QR code read. A connection is opened to the device camera
     * and the photo viewfinder is started.
     */
    int BarcodeScannerNDK::startRead() {

        std::string errorEvent = "community.barcodescanner.errorfound.native";
        Json::FastWriter writer;
        Json::Value root;

        int rc;
		if ((rc = img_lib_attach(&ilib)) != IMG_ERR_OK) {
			fprintf(stderr, "img_lib_attach() failed: %d\n", rc);
		}

        camera_error_t err;

        err = camera_open(CAMERA_UNIT_REAR,CAMERA_MODE_RW | CAMERA_MODE_ROLL,&mCameraHandle);
        if ( err != CAMERA_EOK){
#ifdef DEBUG
            fprintf(stderr, " Ran into an issue when initializing the camera = %d\n ", err);
#endif
            root["state"] = "Open Camera";
            root["error"] = err;
            root["description"] = getCameraErrorDesc( err );
            m_pParent->NotifyEvent(errorEvent + " " + writer.write(root));
            return EIO;
        }

        int numRates = 0;
		err = camera_get_photo_vf_framerates(mCameraHandle, true, 0, &numRates, NULL, NULL);
		double* camFramerates = new double[numRates];
		bool maxmin = false;
		err = camera_get_photo_vf_framerates(mCameraHandle, true, numRates, &numRates, camFramerates, &maxmin);
//		for (int x=0; x<numRates; x++) {
//			root["framerate"] = camFramerates[x];
//			root["maxmin"] = maxmin;
//			m_pParent->NotifyEvent(errorEvent + " " + writer.write(root));
//		}

		uint32_t* rotations = new uint32_t[8];
		int numRotations = 0;
		bool nonsquare = false;
		err = camera_get_photo_rotations(mCameraHandle, CAMERA_FRAMETYPE_JPEG, true, 8, &numRotations, rotations, &nonsquare);
		rotation = rotations[0] / 90;
//		root["rotation"] = rotations[0];
//		root["numRotations"] = numRotations;
//		m_pParent->NotifyEvent(errorEvent + " " + writer.write(root));

		err = camera_set_photovf_property(mCameraHandle,
			CAMERA_IMGPROP_BURSTMODE, 1,
			CAMERA_IMGPROP_FRAMERATE, camFramerates[0]);
		if ( err != CAMERA_EOK){
#ifdef DEBUG
			fprintf(stderr, " Ran into an issue when configuring the camera viewfinder = %d\n ", err);
#endif
			root["state"] = "Set VF Props";
			root["error"] = err;
			root["description"] = getCameraErrorDesc( err );
			m_pParent->NotifyEvent(errorEvent + " " + writer.write(root));
			return EIO;
		}

		err = camera_set_photo_property(mCameraHandle,
			CAMERA_IMGPROP_BURSTDIVISOR, camFramerates[0]);
		if ( err != CAMERA_EOK){
#ifdef DEBUG
			fprintf(stderr, " Ran into an issue when configuring the camera properties = %d\n ", err);
#endif
			root["state"] = "Set Cam Props";
			root["error"] = err;
			root["description"] = getCameraErrorDesc( err );
			m_pParent->NotifyEvent(errorEvent + " " + writer.write(root));
			return EIO;
		}

        err = camera_start_photo_viewfinder( mCameraHandle, &viewfinder_callback, NULL, NULL);
        if ( err != CAMERA_EOK) {
#ifdef DEBUG
            fprintf(stderr, "Ran into a strange issue when starting up the camera viewfinder\n");
#endif
            root["state"] = "ViewFinder Start";
            root["error"] = err;
            root["description"] = getCameraErrorDesc( err );
            m_pParent->NotifyEvent(errorEvent + " " + writer.write(root));
            return EIO;
        }

        err = camera_set_focus_mode(mCameraHandle, CAMERA_FOCUSMODE_CONTINUOUS_MACRO);
		if ( err != CAMERA_EOK){
#ifdef DEBUG
			fprintf(stderr, " Ran into an issue when setting focus mode = %d\n ", err);
#endif
			root["state"] = "Set Focus Mode";
			root["error"] = err;
			root["description"] =  getCameraErrorDesc( err );
			m_pParent->NotifyEvent(errorEvent + " " + writer.write(root));
			return EIO;
		}

		err = camera_start_burst(mCameraHandle, NULL, NULL, NULL, &image_callback, NULL);
		if ( err != CAMERA_EOK) {
#ifdef DEBUG
			fprintf(stderr, "Ran into an issue when starting up the camera in burst mode\n");
#endif
			root["state"] = "Start Camera Burst";
			root["error"] = err;
			root["description"] = getCameraErrorDesc( err );
			m_pParent->NotifyEvent(errorEvent + " " + writer.write(root));
			return EIO;
		}

        std::string successEvent = "community.barcodescanner.started.native";
        root["successful"] = true;
        m_pParent->NotifyEvent(successEvent + " " + writer.write(root));
        return EOK;
    }


    /*
     * BarcodeScannerNDK::stopRead
     *
     * This method is called to clean up following successful detection of a barcode.
     * Calling this method will stop the viewfinder and close an open connection to the device camera.
     */
    int BarcodeScannerNDK::stopRead() {

    	std::string errorEvent = "community.barcodescanner.errorfound.native";
		Json::FastWriter writer;
		Json::Value root;
        camera_error_t err;

        err = camera_stop_burst(mCameraHandle);
		if ( err != CAMERA_EOK)
		{
#ifdef DEBUG
			fprintf(stderr, "Error with turning off the burst \n");
#endif
			root["error"] = err;
			root["description"] = getCameraErrorDesc( err );
			m_pParent->NotifyEvent(errorEvent + " " + writer.write(root));
			return EIO;
		}

        err = camera_stop_photo_viewfinder(mCameraHandle);
        if ( err != CAMERA_EOK)
        {
#ifdef DEBUG
            fprintf(stderr, "Error with turning off the photo viewfinder \n");
#endif
            root["error"] = err;
		    root["description"] = getCameraErrorDesc( err );
		    m_pParent->NotifyEvent(errorEvent + " " + writer.write(root));
            return EIO;
        }

        //check to see if the camera is open, if it is open, then close it
        err = camera_close(mCameraHandle);
        if ( err != CAMERA_EOK){
#ifdef DEBUG
            fprintf(stderr, "Error with closing the camera \n");
#endif
            root["error"] = err;
            root["description"] = getCameraErrorDesc( err );
            m_pParent->NotifyEvent(errorEvent + " " + writer.write(root));
            return EIO;
        }

        img_lib_detach(ilib);

        std::string successEvent = "community.barcodescanner.ended.native";
        root["successful"] = true;
        mCameraHandle = CAMERA_HANDLE_INVALID;
        m_pParent->NotifyEvent(successEvent + " " + writer.write(root));

        return EOK;
    }

    std::string YUV_NV12_TO_RGB(const unsigned char* yuv, int width, int height) {

    	std::string rgbString = "";
    	const int frameSize = width * height;

    	const int di = +1;
    	const int dj = +1;

    	int a = 0;
    	for (int i = 0, ci = 0; i < height; ++i, ci += di) {
    		for (int j = 0, cj = 0; j < width; ++j, cj += dj) {
    			int y = (0xff & ((int) yuv[ci * width + cj]));
    			int u = (0xff & ((int) yuv[frameSize + (ci >> 1) * width + (cj & ~1) + 0]));
    			int v = (0xff & ((int) yuv[frameSize + (ci >> 1) * width + (cj & ~1) + 1]));
    			y = y < 16 ? 16 : y;

    			int r = (int) (1.164f * (y - 16) + 1.596f * (v - 128));
    			int g = (int) (1.164f * (y - 16) - 0.813f * (v - 128) - 0.391f * (u - 128));
    			int b = (int) (1.164f * (y - 16) + 2.018f * (u - 128));

    			r = r < 0 ? 0 : (r > 255 ? 255 : r);
    			g = g < 0 ? 0 : (g > 255 ? 255 : g);
    			b = b < 0 ? 0 : (b > 255 ? 255 : b);

    			rgbString += (char)r;
    			rgbString += (char)g;
    			rgbString += (char)b;
    			//argb[a++] = 0xff000000 | (r << 16) | (g << 8) | b;
    		}
    	}

    	return rgbString;
    }

    std::string getBitmapHeader(const unsigned int width, const unsigned int height) {

    	std::stringstream ss (std::stringstream::in);
    	ss << hex << (width * height);
    	std::string numFileBytes = ss.str();

        ss.str("");
        ss.clear();
    	ss << hex << (width);
    	std::string w = ss.str();

        ss.str("");
        ss.clear();
    	ss << hex << (height);
    	std::string h = ss.str();

    	return "BM" +                    // Signature
    		numFileBytes +            // size of the file (bytes)*
    	    "\x00\x00" +              // reserved
    	    "\x00\x00" +              // reserved
    	    "\x36\x00\x00\x00" +      // offset of where BMP data lives (54 bytes)
    	    "\x28\x00\x00\x00" +      // number of remaining bytes in header from here (40 bytes)
    	    w +                       // the width of the bitmap in pixels*
    	    h +                       // the height of the bitmap in pixels*
    	    "\x01\x00" +              // the number of color planes (1)
    	    "\x20\x00" +              // 32 bits / pixel
    	    "\x00\x00\x00\x00" +      // No compression (0)
    	    "\x00\x00\x00\x00" +      // size of the BMP data (bytes)*
    	    "\x13\x0B\x00\x00" +      // 2835 pixels/meter - horizontal resolution
    	    "\x13\x0B\x00\x00" +      // 2835 pixels/meter - the vertical resolution
    	    "\x00\x00\x00\x00" +      // Number of colors in the palette (keep 0 for 32-bit)
    	    "\x00\x00\x00\x00";       // 0 important colors (means all colors are important)
    }
}
