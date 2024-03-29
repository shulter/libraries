/*
  Copyright (c) 2013 Arduino LLC. All right reserved.

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
*/

#include <FileIO.h>



File::File(BridgeClass &b) : mode(255), bridge(b) {
  // Empty
}

File::File(const char *_filename, uint8_t _mode, BridgeClass &b) : mode(_mode), bridge(b) {
  filename = _filename;
  char modes[] = {'r','w','a'};
  uint8_t cmd[] = {'F', modes[mode]};
  uint8_t res[2];
  dirPosition = 1;
  bridge.transfer(cmd, 2, (uint8_t*)filename.c_str(), filename.length(), res, 2);
  if (res[0] != 0) { // res[0] contains error code
    mode = 255; // In case of error keep the file closed
    return;
  }
  handle = res[1];
  buffered = 0;
}

File::operator bool() {
  return (mode != 255);
}

File::~File() {
  close();
}

size_t File::write(uint8_t c) {
  return write(&c, 1);
}

size_t File::write(const uint8_t *buf, size_t size) {
  if (mode == 255)
	return -1;
  uint8_t cmd[] = {'g', handle};
  uint8_t res[1];
  bridge.transfer(cmd, 2, buf, size, res, 1);
  if (res[0] != 0) // res[0] contains error code
	return -res[0];
  return size;
}

int File::read() {
  doBuffer();
  if (buffered == 0)
    return -1; // no chars available
  else {
    buffered--;
    return buffer[readPos++];
  }
}

int File::peek() {
  doBuffer();
  if (buffered == 0)
    return -1; // no chars available
  else
    return buffer[readPos];
}

boolean File::seek(uint32_t position) {
  uint8_t cmd[] = {
    's',
    handle,
    (position >> 24) & 0xFF,
    (position >> 16) & 0xFF,
    (position >> 8) & 0xFF,
    position & 0xFF
  };
  uint8_t res[1];
  bridge.transfer(cmd, 6, res, 1);
  if (res[0]==0) {
    // If seek succeed then flush buffers
    buffered = 0;
    return true;
  }
  return false;
}

uint32_t File::position() {
  uint8_t cmd[] = {'S', handle};
  uint8_t res[5];
  bridge.transfer(cmd, 2, res, 5);
  //err = res[0]; // res[0] contains error code
  uint32_t pos = res[1] << 24;
  pos += res[2] << 16;
  pos += res[3] << 8;
  pos += res[4];
  return pos - buffered;
}

void File::doBuffer() {
  // If there are already char in buffer exit
  if (buffered > 0)
    return;

  // Try to buffer up to BUFFER_SIZE characters
  readPos = 0;
  uint8_t cmd[] = {'G', handle, BUFFER_SIZE - 1};
  buffered = bridge.transfer(cmd, 3, buffer, BUFFER_SIZE) - 1;
  //err = buff[0]; // First byte is error code
  if (buffered>0) {
    // Shift the reminder of buffer
    for (uint8_t i=0; i<buffered; i++)
      buffer[i] = buffer[i+1];
  }
}

int File::available() {
  // Look if there is new data available
  doBuffer();
  return buffered;
}

void File::flush() {
}

//int read(void *buf, uint16_t nbyte)

//uint32_t size()

void File::close() {
  if (mode == 255)
    return;
  uint8_t cmd[] = {'f', handle};
  bridge.transfer(cmd, 2);
  mode = 255;
}

const char *File::name() {
  return filename.c_str();
}


boolean File::isDirectory() {
  uint8_t res[1];
  uint8_t lenght;
  uint8_t cmd[] = {'i'};
  if (mode != 255)
    return 0;

  bridge.transfer(cmd, 1, (uint8_t *)filename.c_str(), filename.length(), res, 1);
  return res[0];
}


File File::openNextFile(uint8_t mode){
  Process awk;
  char tmp;
  String command;
  String filepath;
  if (dirPosition == 0xFFFF) return File();

  command = "ls ";
  command += filename;
  command += " | awk 'NR==";
  command += dirPosition;
  command += "'";
  
  awk.runShellCommand(command);

  while(awk.running()); 

  command = "";

  while (awk.available()){
    tmp = awk.read();
    if (tmp!='\n') command += tmp;
  }
  if (command.length() == 0)
    return File();
  dirPosition++;
  filepath = filename + "/" + command;
  return File(filepath.c_str(),mode);
  
} 

void File::rewindDirectory(void){
  dirPosition = 1;
}






boolean FileSystemClass::begin() {
  return true;
}

File FileSystemClass::open(const char *filename, uint8_t mode) {
  return File(filename, mode);
}

boolean FileSystemClass::exists(const char *filepath) {
  Process ls;
  ls.begin("ls");
  ls.addParameter(filepath);
  int res = ls.run();
  return (res == 0);
}

boolean FileSystemClass::mkdir(const char *filepath) {
  Process mk;
  mk.begin("mkdir");
  mk.addParameter("-p");
  mk.addParameter(filepath);
  int res = mk.run();
  return (res == 0);
}

boolean FileSystemClass::remove(const char *filepath) {
  Process rm;
  rm.begin("rm");
  rm.addParameter(filepath);
  int res = rm.run();
  return (res == 0);
}

boolean FileSystemClass::rmdir(const char *filepath) {
  Process rm;
  rm.begin("rmdir");
  rm.addParameter(filepath);
  int res = rm.run();
  return (res == 0);
}

FileSystemClass FileSystem;
