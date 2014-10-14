#ifndef CHOWDREN_FILEIO_H
#define CHOWDREN_FILEIO_H

#include <string>
#include <stdio.h>

class BaseFile
{
public:
    void * handle;
    bool closed;

    BaseFile();
    BaseFile(const char * filename, const char * mode);
    ~BaseFile();
    void open(const char * filename, const char * mode);
    bool seek(size_t v, int origin = SEEK_SET);
    size_t tell();
    bool is_open();
    size_t read(void * data, size_t size);
    size_t write(const void * data, size_t size);
    void close();
    bool at_end();
};

class BufferedFile
{
public:
    BaseFile fp;

    size_t size;
    size_t pos;
    size_t buf_pos;
    size_t buf_size;
    void * buffer;

    BufferedFile();
    BufferedFile(const char * filename, const char * mode);
    ~BufferedFile();
    void buffer_data(size_t size);
    void open(const char * filename, const char * mode);
    bool seek(size_t v, int origin = SEEK_SET);
    size_t tell();
    bool is_open();
    size_t read(void * data, size_t size);
    size_t write(const void * data, size_t size);
    void close();
    bool at_end();
};

#ifdef CHOWDREN_FILE_BUFFERING
typedef BufferedFile FSFile;
#else
typedef BaseFile FSFile;
#endif

bool read_file(const char * filename, char ** data, size_t * ret_size,
               bool binary = true);
bool read_file(const char * filename, std::string & dst,
               bool binary = true);
bool read_file_c(const char * filename, char ** data, size_t * ret_size,
                 bool binary = true);

extern "C" {
void * fsfile_fopen(const char * filename, const char * mode);
int fsfile_fclose(void * fp);
int fsfile_fseek(void * fp, long int offset, int origin);
size_t fsfile_fread(void * ptr, size_t size, size_t count, void * fp);
long int fsfile_ftell(void * fp);
}

#endif // CHOWDREN_FILEIO_H
