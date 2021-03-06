#include "pfm.h"
#include <iostream>

using namespace std;

PagedFileManager *PagedFileManager::_pf_manager = nullptr;

PagedFileManager &PagedFileManager::instance() {
    static PagedFileManager _pf_manager = PagedFileManager();
    return _pf_manager;
}

PagedFileManager::PagedFileManager() = default;

PagedFileManager::~PagedFileManager() { delete _pf_manager; }

PagedFileManager::PagedFileManager(const PagedFileManager &) = default;

PagedFileManager &PagedFileManager::operator=(const PagedFileManager &) = default;

RC PagedFileManager::createFile(const std::string &fileName) {
    //File already exists!
    if(access(fileName.c_str(),F_OK) == 0)
        return -1;
    
    FILE *fp = fopen(fileName.c_str(),"wb+");
    //File open error!
    if(!fp)
        return -1;

    byte cnt[PAGE_SIZE];
    memset(cnt,0,sizeof(cnt));
    fseek(fp,0,SEEK_SET);
    fwrite(cnt,1,PAGE_SIZE,fp);
    //if fp is not closed,there could be undefined behaviors, e.g.  counter value undefined.
    //This is because when two file descriptors exist concurrently, they read like the other never writes.(they read original data)
    fclose(fp);
    return 0;
}

RC PagedFileManager::destroyFile(const std::string &fileName) {
    return remove(fileName.c_str());
}

RC PagedFileManager::openFile(const std::string &fileName, FileHandle &fileHandle) {
    //File doesn't exist!
    if(access(fileName.c_str(),F_OK)) {
        return -1;
    }

    //fileHandle is already a handle for some open file!
    if(fileHandle.fp) {
        return -1;
    }

    fileHandle.fp = fopen(fileName.c_str(),"rb+");
    //File open error!
    if(!fileHandle.fp) {
        return -1;
    }
    
    unsigned cnt[5];
    fseek(fileHandle.fp,0,SEEK_SET);
    fread(cnt,sizeof(unsigned),5,fileHandle.fp);
    fileHandle.readPageCounter = cnt[0];
    fileHandle.writePageCounter =  cnt[1];
    fileHandle.appendPageCounter = cnt[2];
    fileHandle.noPages = cnt[3];
    fileHandle.lastTableID = cnt[4];
    return 0;
}

RC PagedFileManager::closeFile(FileHandle &fileHandle) {
    if(fileHandle.fp == NULL) {
        return -1;
    }
    fseek(fileHandle.fp, 0, SEEK_SET);
    unsigned cnt[5];
    cnt[0] = fileHandle.readPageCounter;
    cnt[1] = fileHandle.writePageCounter;
    cnt[2] = fileHandle.appendPageCounter;
    cnt[3] = fileHandle.noPages;
    cnt[4] = fileHandle.lastTableID;
    fwrite(cnt,sizeof(unsigned),5,fileHandle.fp);
    //File close error!
    if(fclose(fileHandle.fp) == EOF) {
        return -1;
    }
    fileHandle.fp = NULL;
    return 0;
}

FileHandle::FileHandle() {
    readPageCounter = 0;
    writePageCounter = 0;
    appendPageCounter = 0;
    noPages = 0;
    lastTableID = 0;
    fp = NULL;
}

FileHandle::~FileHandle() = default;

RC FileHandle::readPage(PageNum pageNum, void *data) {
    //Intend to read a non-existing page!
    if(pageNum >= noPages){
        readPageCounter++;
        return -1;
    }
    
    fseek( fp, (pageNum+1)*PAGE_SIZE, SEEK_SET );
    unsigned res = fread(data,sizeof(char),PAGE_SIZE,fp);
    //File read error!
    if(res != PAGE_SIZE && feof(fp) != EOF)
        return -1;
    
    readPageCounter++;
    return 0;
}

RC FileHandle::writePage(PageNum pageNum, const void *data) {
    //Intend to update a non-existing page!    
    //Fix test case 06
    if(pageNum >= noPages){
        writePageCounter++;
        return -1;
    }
    
    fseek( fp, (pageNum+1)*PAGE_SIZE, SEEK_SET );
    unsigned res = fwrite(data,sizeof(char),PAGE_SIZE,fp);
    //File write error!
    if(res != PAGE_SIZE)
        return -1;
    
    writePageCounter++;
    return 0;
}

RC FileHandle::appendPage(const void *data) {
    fseek( fp, 0, SEEK_END );
    unsigned res = fwrite(data,sizeof(char),PAGE_SIZE,fp);
    //File append error!
    if(res != PAGE_SIZE)
        return -1;
    
    noPages++;
    appendPageCounter++;
    return 0;
}

unsigned FileHandle::getNumberOfPages() {
    return noPages;
}

RC FileHandle::collectCounterValues(unsigned &readPageCount, unsigned &writePageCount, unsigned &appendPageCount) {
    readPageCount = readPageCounter;
    writePageCount = writePageCounter;
    appendPageCount = appendPageCounter;
    return 0;
}

unsigned int FileHandle::getLastTableId() const {
    return lastTableID;
}

void FileHandle::setLastTableId(unsigned int lastTableId) {
    lastTableID = lastTableId;
}
