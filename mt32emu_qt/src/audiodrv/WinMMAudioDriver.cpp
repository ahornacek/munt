/* Copyright (C) 2011 Jerome Fisher, Sergey V. Mikayev
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License as published by
 *  the Free Software Foundation, either version 2.1 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "WinMMAudioDriver.h"
#include "../QSynth.h"
#include <process.h>

using namespace MT32Emu;

#define	MIN_RENDER_SAMPLES 32	// render at least this number of samples

// SergM: 100 ms output latency is safe on most systems.
// can be reduced to 35 ms (works on my system)
// 30 ms is the absolute minimum, unavoidable KSMixer latency
static const int FRAMES_IN_BUFFER = 3200; // FIXME: should be a variable
static const int FRAME_SIZE = 4; // Stereo, 16-bit
static const int latency = 1000000; // 1 ms latency for MIDI processing

WinMMAudioDriver::WinMMAudioDriver(QSynth *useSynth, unsigned int useSampleRate) : 
	synth(useSynth), sampleRate(useSampleRate), hWaveOut(NULL), pendingClose(false) {
		buffer = new Bit16s[FRAME_SIZE * FRAMES_IN_BUFFER];
}

WinMMAudioDriver::~WinMMAudioDriver() {
	if (hWaveOut != NULL) {
		close();
	}
	delete[] buffer;
}

SynthTimestamp WinMMAudioDriver::getPlayedAudioNanosPlusLatency() {
	if (hWaveOut == NULL) {
		qDebug() << "hWaveOut is NULL at getPlayedAudioNanosPlusLatency()";
		return 0;
	}

	MMTIME mmTime;
	mmTime.wType = TIME_SAMPLES;

	// FIXME: waveOutGetPosition() wraps after 2^27 frames played (about a hour at 32000 Hz)
	// Hopefully it'll be removed as deprecated eventually
	waveOutGetPosition(hWaveOut, &mmTime, sizeof MMTIME);
	return ((double)mmTime.u.sample + FRAMES_IN_BUFFER)
		/ sampleRate * MasterClock::NANOS_PER_SECOND;
}

void WinMMAudioDriver::processingThread(void *userData) {
	DWORD renderPos = 0;
	DWORD playCursor, frameCount;
	MMTIME mmTime;
	WinMMAudioDriver *driver = (WinMMAudioDriver *)userData;
	while (!driver->pendingClose) {
		mmTime.wType = TIME_SAMPLES;
		waveOutGetPosition(driver->hWaveOut, &mmTime, sizeof MMTIME);
		playCursor = mmTime.u.sample % FRAMES_IN_BUFFER;
		if (playCursor < renderPos) {
			frameCount = FRAMES_IN_BUFFER - renderPos;
		} else {
			frameCount = playCursor - renderPos;
			if (frameCount < MIN_RENDER_SAMPLES) {
				Sleep(1);
				continue;
			}
		}
		unsigned int rendered = driver->synth->render(driver->buffer + (renderPos << 1), frameCount,
			MasterClock::getClockNanos() - latency, driver->sampleRate);
		if (rendered < frameCount) { // SergM: never found this true
			char *out = (char *)driver->buffer + FRAME_SIZE * renderPos;
			// Fill this with 0 due to the real synth fault
			memset(out + rendered * FRAME_SIZE, 0, (frameCount - rendered) * FRAME_SIZE);
		}
		renderPos += frameCount;
		if (renderPos >= FRAMES_IN_BUFFER) {
			renderPos -= FRAMES_IN_BUFFER;
		}
	}
	driver->pendingClose = false;
	return;
}

QList<QString> WinMMAudioDriver::getDeviceNames() {
	QList<QString> deviceNames;
	UINT deviceCount = waveOutGetNumDevs();
	if (deviceCount == 0) {
		qDebug() << "No waveOut devices found";
		deviceCount = 0;
	}
	for(UINT deviceIndex = 0; deviceIndex < deviceCount; deviceIndex++) {
		WAVEOUTCAPS deviceInfo;
		if (waveOutGetDevCaps(deviceIndex, &deviceInfo, sizeof(deviceInfo)) != MMSYSERR_NOERROR) {
			qDebug() << "waveOutGetDevCaps failed for" << deviceIndex;
			continue;
		}
		qDebug() << " Device:" << deviceIndex << deviceInfo.szPname;
		deviceNames << deviceInfo.szPname;
	}
	return deviceNames;
}

bool WinMMAudioDriver::start(int deviceIndex) {
	if (buffer == NULL) {
		return false;
	}
	memset(buffer, 0, FRAME_SIZE * FRAMES_IN_BUFFER);
	if (hWaveOut != NULL) {
		close();
	}
	if (deviceIndex < 0) {
		deviceIndex = WAVE_MAPPER;
		qDebug() << "Using default WaveOut device";
	} else {
		qDebug() << "Using WaveOut device:" << deviceIndex;
	}

	PCMWAVEFORMAT wFormat = {WAVE_FORMAT_PCM, /* stereo */ 2, sampleRate,
		sampleRate * FRAME_SIZE, FRAME_SIZE, /* BitsPerSample */ 16};

	//	Open waveout device
	if (waveOutOpen(&hWaveOut, deviceIndex, (LPWAVEFORMATEX)&wFormat, NULL,
		NULL, CALLBACK_NULL) != MMSYSERR_NOERROR) {
			qDebug() << "waveOutOpen failed";
			return false;
	}

	//	Prepare header
	WaveHdr.dwBufferLength = FRAME_SIZE * FRAMES_IN_BUFFER;
	WaveHdr.lpData = (LPSTR)buffer;
	WaveHdr.dwFlags = WHDR_BEGINLOOP | WHDR_ENDLOOP;
	WaveHdr.dwLoops = -1;
	if (waveOutPrepareHeader(hWaveOut, &WaveHdr, sizeof(WAVEHDR)) != MMSYSERR_NOERROR) {
		qDebug() << "waveOutPrepareHeader failed";
		waveOutClose(hWaveOut);
		return false;
	}

	//	Start playing
	if (waveOutWrite(hWaveOut, &WaveHdr, sizeof(WAVEHDR)) != MMSYSERR_NOERROR) {
		qDebug() << "waveOutWrite failed";
		waveOutUnprepareHeader(hWaveOut, &WaveHdr, sizeof(WAVEHDR));
		waveOutClose(hWaveOut);
		return false;
	}
	_beginthread(processingThread, 1024*1024, this);
	return true;
}

void WinMMAudioDriver::close() {
	if (hWaveOut != NULL) {
		pendingClose = true;
		waveOutReset(hWaveOut);
		waveOutUnprepareHeader(hWaveOut, &WaveHdr, sizeof(WAVEHDR));
		waveOutClose(hWaveOut);
		hWaveOut = NULL;
		qDebug() << "Stopping processing thread...";
		while (pendingClose) {
			Sleep(1);
		}
	}
	return;
}
