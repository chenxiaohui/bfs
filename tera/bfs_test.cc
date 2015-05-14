// Copyright (c) 2015, Baidu.com, Inc. All Rights Reserved
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Author: yanshiguang02@baidu.com

#include "dfs.h"

#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>

const char* so_path = "./bfs_wrapper.so";
const char* dfs_conf = "yq01-tera60.yq01:8828";

const std::string test_path("/test");

#define ASSERT_TRUE(x) \
    do { \
        bool ret = x; \
        if (!ret) { \
            printf("\033[31m[Test failed]\033[0m: %d, %s\n", __LINE__, #x); \
            exit(1); \
        } else { \
            printf("\033[32m[Test pass]\033[0m: %s\n", #x); \
        } \
    } while(0)

int main() {
    dlerror();
    void* handle = dlopen(so_path, RTLD_NOW);
    const char* err = dlerror();
    if (handle == NULL) {
        fprintf(stderr, "Open %s fail: %s\n", so_path, err);
        return 1;
    }

    leveldb::DfsCreator creator = (leveldb::DfsCreator)dlsym(handle, "NewDfs");
    err = dlerror();
    if (err != NULL) {
        fprintf(stderr, "Load NewDfs from %s fail: %s\n", so_path, err);
        return 1;
    }

    leveldb::Dfs* dfs = (*creator)(dfs_conf);
    
    /// Create dir
    ASSERT_TRUE(dfs != NULL);
    ASSERT_TRUE(0 == dfs->CreateDirectory(test_path));

    /// Open
    std::string file1 = test_path + "/file1";
    std::string file2 = test_path + "/file2";
    std::string file3 = test_path + "/file3";

    if (0 == dfs->Exists(file1)) {
        ASSERT_TRUE(0 == dfs->Delete(file1));
    }

    leveldb::DfsFile* fp = dfs->OpenFile(file1, leveldb::WRONLY);
    ASSERT_TRUE(fp != NULL);
    
    /// Write&Sync
    char content1[] = "File1 content";
    char content2[] = "Content for read";
    const int c1len = sizeof(content1);
    const int c2len = sizeof(content2);
    ASSERT_TRUE(c1len == fp->Write(content1, c1len));
    ASSERT_TRUE(0 == fp->Flush());
    ASSERT_TRUE(0 == fp->Sync());
    ASSERT_TRUE(c2len == fp->Write(content2, c2len));

    long lbuf[1024];
    int buflen = sizeof(lbuf);
    int bufnum = 1024;
    for (int i = 0; i < 1024; i++) {
        for (int j = 0; j < 1024; j++) {
            lbuf[j] = ((rand() << 16) | rand());
        }
        buflen = sizeof(lbuf);
        ASSERT_TRUE(buflen == fp->Write(reinterpret_cast<char*>(lbuf), buflen));
    }

    /// Close
    ASSERT_TRUE(0 == fp->CloseFile());
    delete fp;
    fp = NULL;

    /// Rename
    if (0 == dfs->Exists(file2)) {
        ASSERT_TRUE(0 == dfs->Delete(file2));
    }
    ASSERT_TRUE(0 == dfs->Rename(file1, file2));
    ASSERT_TRUE(0 == dfs->Exists(file2));
    ASSERT_TRUE(0 != dfs->Exists(file1));

    /// GetFileSize
    uint64_t fsize = 0;
    ASSERT_TRUE(0 == dfs->GetFileSize(file2, &fsize));
    ASSERT_TRUE(c1len + c2len + buflen * bufnum == static_cast<int>(fsize));

    /// Read
    fp = dfs->OpenFile(file2, leveldb::RDONLY);
    ASSERT_TRUE(fp != NULL);
    char buf[128];
    ASSERT_TRUE(c1len == fp->Read(buf, c1len));
    ASSERT_TRUE(0 == strncmp(buf, content1, c1len));
    ASSERT_TRUE(c2len == fp->Read(buf, c2len));
    printf("content2= %s, content_read= %s\n", content2, buf);
    ASSERT_TRUE(0 == strncmp(buf, content2, c2len));
    ASSERT_TRUE(c2len == fp->Pread(c1len, buf, c2len));
    ASSERT_TRUE(0 == strncmp(buf, content2, c2len));
    ASSERT_TRUE(0 == fp->CloseFile());
    delete fp;
    fp = NULL;
    
    /// Sync
    dfs->Delete(file3);
    fp = dfs->OpenFile(file3, leveldb::WRONLY);
    ASSERT_TRUE(fp != NULL);
    ASSERT_TRUE(c1len == fp->Write(content1, c1len));
    ASSERT_TRUE(0 == fp->Flush());
    ASSERT_TRUE(0 == fp->Sync());
    ASSERT_TRUE(c2len == fp->Write(content2, c2len));
    
    leveldb::DfsFile* nfp = dfs->OpenFile(file3, leveldb::RDONLY);
    ASSERT_TRUE(nfp != NULL);
    ASSERT_TRUE(c1len == nfp->Read(buf, c1len));
    ASSERT_TRUE(0 == strncmp(buf, content1, c1len));
    ASSERT_TRUE(0 == nfp->CloseFile());

    /// List directory
    std::vector<std::string> result;
    ASSERT_TRUE(0 == dfs->ListDirectory(test_path, &result));
    ASSERT_TRUE(2 == result.size());
    ASSERT_TRUE(file2 == result[0]);

    /// Delete
    ASSERT_TRUE(0 == dfs->Delete(file2));
    ASSERT_TRUE(0 != dfs->Exists(file2));

    /// Delete directory
    result.clear();
    ASSERT_TRUE(0 == dfs->DeleteDirectory(test_path));
    ASSERT_TRUE(0 == dfs->ListDirectory(test_path, &result));
    ASSERT_TRUE(0 == result.size());
}

/* vim: set expandtab ts=4 sw=4 sts=4 tw=100: */
