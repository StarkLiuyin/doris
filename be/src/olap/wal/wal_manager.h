// Licensed to the Apache Software Foundation (ASF) under one
// or more contributor license agreements.  See the NOTICE file
// distributed with this work for additional information
// regarding copyright ownership.  The ASF licenses this file
// to you under the Apache License, Version 2.0 (the
// "License"); you may not use this file except in compliance
// with the License.  You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing,
// software distributed under the License is distributed on an
// "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied.  See the License for the
// specific language governing permissions and limitations
// under the License.

#pragma once

#include <gen_cpp/PaloInternalService_types.h>

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <shared_mutex>
#include <thread>
#include <unordered_map>

#include "common/config.h"
#include "gen_cpp/FrontendService.h"
#include "gen_cpp/FrontendService_types.h"
#include "gen_cpp/HeartbeatService_types.h"
#include "gutil/ref_counted.h"
#include "olap/wal/wal_dirs_info.h"
#include "olap/wal/wal_reader.h"
#include "olap/wal/wal_table.h"
#include "olap/wal/wal_writer.h"
#include "runtime/exec_env.h"
#include "runtime/stream_load/stream_load_context.h"
#include "util/thread.h"
#include "util/threadpool.h"

namespace doris {
class WalManager {
    ENABLE_FACTORY_CREATOR(WalManager);

public:
    enum WalStatus {
        PREPARE = 0,
        REPLAY = 1,
        CREATE = 2,
    };

public:
    WalManager(ExecEnv* exec_env, const std::string& wal_dir);
    ~WalManager();
    Status init();
    bool is_running();
    void stop();

    // wal back pressure
    Status update_wal_dir_limit(const std::string& wal_dir, size_t limit = -1);
    Status update_wal_dir_used(const std::string& wal_dir, size_t used = -1);
    Status update_wal_dir_pre_allocated(const std::string& wal_dir, size_t increase_pre_allocated,
                                        size_t decrease_pre_allocated);
    Status get_wal_dir_available_size(const std::string& wal_dir, size_t* available_bytes);
    size_t get_max_available_size();

    // replay wal
    Status create_wal_path(int64_t db_id, int64_t table_id, int64_t wal_id,
                           const std::string& label, std::string& base_path);
    Status get_wal_path(int64_t wal_id, std::string& wal_path);
    Status delete_wal(int64_t wal_id, size_t block_queue_pre_allocated = 0);
    Status add_recover_wal(int64_t db_id, int64_t table_id, int64_t wal_id, std::string wal);
    // used for ut
    size_t get_wal_table_size(int64_t table_id);
    // TODO util function, should remove
    Status create_wal_reader(const std::string& wal_path, std::shared_ptr<WalReader>& wal_reader);
    Status create_wal_writer(int64_t wal_id, std::shared_ptr<WalWriter>& wal_writer);

    // for wal status, can be removed
    Status get_wal_status_queue_size(const PGetWalQueueSizeRequest* request,
                                     PGetWalQueueSizeResponse* response);
    void add_wal_status_queue(int64_t table_id, int64_t wal_id, WalStatus wal_status);
    Status erase_wal_status_queue(int64_t table_id, int64_t wal_id);
    void print_wal_status_queue();

    // for _wal_column_id_map, can be removed
    void add_wal_column_index(int64_t wal_id, std::vector<size_t>& column_index);
    void erase_wal_column_index(int64_t wal_id);
    Status get_wal_column_index(int64_t wal_id, std::vector<size_t>& column_index);

private:
    // wal back pressure
    Status _init_wal_dirs_conf();
    Status _init_wal_dirs();
    Status _init_wal_dirs_info();
    std::string _get_base_wal_path(const std::string& wal_path_str);
    Status _update_wal_dir_info_thread();

    // replay wal
    Status _scan_wals(const std::string& wal_path);
    Status _replay();
    void _stop_relay_wal();

public:
    // used for be ut
    size_t wal_limit_test_bytes;

    const std::string tmp = "tmp";

private:
    ExecEnv* _exec_env = nullptr;
    std::atomic<bool> _stop;
    CountDownLatch _stop_background_threads_latch;

    // wal back pressure
    std::vector<std::string> _wal_dirs;
    scoped_refptr<Thread> _update_wal_dirs_info_thread;
    std::unique_ptr<WalDirsInfo> _wal_dirs_info;

    // replay wal
    scoped_refptr<Thread> _replay_thread;
    std::unique_ptr<doris::ThreadPool> _thread_pool;

    std::shared_mutex _table_lock;
    std::map<int64_t, std::shared_ptr<WalTable>> _table_map;

    std::shared_mutex _wal_lock;
    std::unordered_map<int64_t, std::string> _wal_path_map;

    // TODO Now only used for debug wal status, consider remove it
    std::shared_mutex _wal_status_lock;
    std::unordered_map<int64_t, std::unordered_map<int64_t, WalStatus>> _wal_status_queues;

    // TODO should remove
    std::shared_mutex _wal_column_id_map_lock;
    std::unordered_map<int64_t, std::vector<size_t>&> _wal_column_id_map;
};
} // namespace doris