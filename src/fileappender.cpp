/*
 * Copyright 2003,2004 The Apache Software Foundation.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <log4cxx/fileappender.h>
#include <log4cxx/helpers/stringhelper.h>
#include <log4cxx/helpers/loglog.h>
#include <log4cxx/helpers/optionconverter.h>
#include <log4cxx/helpers/synchronized.h>
#include <log4cxx/helpers/transcoder.h>
#include <apr_atomic.h>
#include <apr_file_io.h>

using namespace log4cxx;
using namespace log4cxx::helpers;
using namespace log4cxx::spi;

IMPLEMENT_LOG4CXX_OBJECT(FileAppender)

FileAppender::FileAppender()
: fileAppend(true), bufferedIO(false), bufferSize(8*1024),
   fileName(), ofs(NULL), fileClosed(1)
{
}

FileAppender::FileAppender(const LayoutPtr& layout, const File& fileName,
	bool append, bool bufferedIO, int bufferSize)
: fileAppend(append), bufferedIO(bufferedIO), bufferSize(bufferSize),
  fileName(fileName), ofs(NULL), fileClosed(1)
{
	this->layout = layout;
}

FileAppender::FileAppender(const LayoutPtr& layout, const File& fileName,
	bool append)
: fileAppend(append), bufferedIO(false), bufferSize(8*1024),
  fileName(fileName), ofs(NULL), fileClosed(1)
{
	this->layout = layout;
}

FileAppender::FileAppender(const LayoutPtr& layout, const File& fileName)
: fileAppend(true), bufferedIO(false), bufferSize(8*1024),
  fileName(fileName), ofs(NULL), fileClosed(1)
{
	this->layout = layout;
}

FileAppender::~FileAppender()
{
	finalize();
}

void FileAppender::setFile(const File& file)
{
	fileName = file;
}

void FileAppender::setFile(const File& file, bool append,
        bool bufferedIO, int bufferSize) {
        fileName = file;
        fileAppend = append;
        this->bufferedIO = bufferedIO;
        this->bufferSize = bufferSize;
}


void FileAppender::closeWriter() {
      apr_file_close(ofs);
      ofs = NULL;
}



void FileAppender::setBufferedIO(bool bufferedIO)
{
	this->bufferedIO = bufferedIO;
	if(bufferedIO)
	{
		immediateFlush = false;
	}
}

void FileAppender::setOption(const LogString& option,
	const LogString& value)
{
	if (StringHelper::equalsIgnoreCase(option, LOG4CXX_STR("FILE"), LOG4CXX_STR("file"))
		|| StringHelper::equalsIgnoreCase(option, LOG4CXX_STR("FILENAME"), LOG4CXX_STR("filename")))
	{
		fileName = value;
	}
	else if (StringHelper::equalsIgnoreCase(option, LOG4CXX_STR("APPEND"), LOG4CXX_STR("append")))
	{
		fileAppend = OptionConverter::toBoolean(value, true);
	}
	else if (StringHelper::equalsIgnoreCase(option, LOG4CXX_STR("BUFFEREDIO"), LOG4CXX_STR("bufferedio")))
	{
		bufferedIO = OptionConverter::toBoolean(value, true);
	}
	else if (StringHelper::equalsIgnoreCase(option, LOG4CXX_STR("IMMEDIATEFLUSH"), LOG4CXX_STR("immediateflush")))
	{
		bufferedIO = !OptionConverter::toBoolean(value, false);
	}
	else if (StringHelper::equalsIgnoreCase(option, LOG4CXX_STR("BUFFERSIZE"), LOG4CXX_STR("buffersize")))
	{
		bufferSize = OptionConverter::toFileSize(value, 8*1024);
	}
	else
	{
		WriterAppender::setOption(option, value);
	}
}

void FileAppender::activateOptions(apr_pool_t* p)
{
        if (fileName.getName().empty()) {
          LogLog::warn((LogString) LOG4CXX_STR("File option not set for appender [")
              + name + LOG4CXX_STR("]."));
          LogLog::warn(LOG4CXX_STR("Are you using FileAppender instead of ConsoleAppender?"));
        } else {
          synchronized sync(mutex);
          if (ofs != NULL) {
            LogLog::warn((LogString) LOG4CXX_STR("Appender [") +
                 name + LOG4CXX_STR("] already open."));
          }
          apr_fileperms_t perm = APR_OS_DEFAULT;
          apr_int32_t flags = APR_WRITE | APR_CREATE;
          if (fileAppend) {
            flags |= APR_APPEND;
          } else {
            flags |= APR_TRUNCATE;
          }
          if (bufferedIO) {
            flags |= APR_BUFFERED;
          }
          ofs = NULL;
          apr_status_t rv = apr_file_open(&ofs,
              fileName.getMBCSName().c_str(), flags, perm, p);
          fileClosed = 0;
        }
        if (ofs != NULL) {
          writeHeader(p);
        }
}

void FileAppender::subAppend(const char* encoded, apr_size_t size, apr_pool_t* p) {
  if (ofs != NULL) {
    apr_status_t rv = apr_file_write(ofs, encoded, &size);
    if (rv == APR_SUCCESS && immediateFlush) {
      rv = apr_file_flush(ofs);
    }
  }
}


