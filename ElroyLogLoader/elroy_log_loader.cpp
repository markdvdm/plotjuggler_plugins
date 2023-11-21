#include "elroy_log_loader.h"
#include "PcapLoader/pcap_loader.h"

#include <QTextStream>
#include <QFile>
#include <QMessageBox>
#include <QDebug>
#include <QSettings>
#include <QProgressDialog>
#include <QDateTime>
#include <QInputDialog>
#include <thread>
#include <sqlite3.h>
#include <cstring>

#include "elroy_common_msg/msg_handling/msg_decoder.h"


using EcmMessageMap = std::unordered_map<std::string, std::variant<std::string, double, bool>>;
ElroyLogLoader::ElroyLogLoader(){
    _extensions.push_back("elroy_log");
}
void ElroyLogLoader::WriteToPlotjugglerThreadSafe(const QString& field_name, const std::variant<std::string, double, bool> &data, double timestamp){
  std::lock_guard<std::mutex> lock(_plotjuggler_mutex);
  //Add a new column to the plotter if it does not already exist
  if (_plots_map.find(field_name) == _plots_map.end() && (std::holds_alternative<double>(data) || std::holds_alternative<bool>(data))){
    auto it = _plot_data->addNumeric(field_name.toStdString());
    _plots_map[field_name] = (&(it->second));
  }
  else if (_string_map.find(field_name) == _string_map.end() && std::holds_alternative<std::string>(data) ){
    auto it = _plot_data->addStringSeries(field_name.toStdString());
    _string_map[field_name] = (&(it->second));
  }
  // Add the data to the plotter
  if(std::holds_alternative<double>(data)){
    PlotData::Point point(timestamp, std::get<double>(data));
    _plots_map[field_name]->pushBack(point);
  }else if(std::holds_alternative<std::string>(data)){
    _string_map[field_name]->pushBack({timestamp, std::get<std::string>(data)});
  }
}
std::vector<EcmMessageMap> ElroyLogLoader::ParseToEcmMap(const uint8_t* const raw_data, size_t byte_array_len, const std::string& delim){
  size_t bytes_processed = 0;
  size_t current_index = 0;
  std::vector<EcmMessageMap> maps;
  while (current_index <= byte_array_len){
    if (current_index >= byte_array_len)
      break;
    EcmMessageMap map;
    elroy_common_msg::MessageDecoderResult res;
    if (!elroy_common_msg::MsgDecoder::DecodeAsMap(raw_data + current_index , byte_array_len-current_index, bytes_processed, map, res, delim)){
      break;
    }
    current_index += bytes_processed;
    maps.push_back(map);
  }
  return maps;
}
bool ElroyLogLoader::ParseEcmToPlotjuggler(const uint8_t* raw_data, size_t byte_array_len, const std::string &delim){
  size_t bytes_processed = 0;
  size_t current_index = 0;
  while (current_index <= byte_array_len){
    if (current_index >= byte_array_len)
      break;
    EcmMessageMap map;
    elroy_common_msg::MessageDecoderResult res;
    if (!elroy_common_msg::MsgDecoder::DecodeAsMap(raw_data + current_index , byte_array_len-current_index, bytes_processed, map, res, delim)){
      break;
    }
    current_index += bytes_processed;
    // Find the instance ID and rename
    if (map.size() == 0)
      continue;
    // Get the instance id
    std::string instance_key = "component" + delim + "instance"; 
    std::string instance_id = "";
    double instance_component = 0;
    for(auto it = map.begin(); it!= map.end(); ++it){
      const std::string& key = it->first;
      if(key.size() > instance_key.size() && key.compare(key.size() - instance_key.size(), instance_key.size(), instance_key) == 0 ){
        instance_id = "_"+  std::to_string(static_cast<size_t>(std::get<double>(map.at(key))));
      }
    }
    // Get the timestamp
    double timestamp = 0;
    std::string timestamp_key = "BusObject" + delim + "write_timestamp_ns";
    for(auto it = map.begin(); it!= map.end(); ++it){
      const std::string& key = it->first;
      if(key.size() > timestamp_key.size() && key.compare(key.size() - timestamp_key.size(), timestamp_key.size(), timestamp_key) == 0 ){
        timestamp = std::get<double>(map.at(key)) / 1e9;
      }
    }
    // Write each element from the ecm message map to plotjuggler
    for (const auto& pair: map){
      std::string field_name_str = pair.first;
      field_name_str = field_name_str.insert(field_name_str.find(delim), "_"+instance_id);
      QString field_name = QString::fromStdString(field_name_str);
      // WriteToPlotjugglerThreadSafe(field_name, pair.second, timestamp);
    }
  }
  return true;
}
bool ElroyLogLoader::MultithreadProcess(std::vector<BlobData> &data, size_t start_idx, size_t end_idx, size_t n_threads, std::string delim){
  // This function should start multiple threads and process data in order
  std::vector<std::vector<EcmMessageMap>> messages(n_threads);
  std::vector<std::thread> threads;
  const size_t chunkSize = data.size() / n_threads;
  const size_t numRows = end_idx - start_idx; // number of rows in chunk
  size_t n  = 0;
  std::mutex mux;
  std::cout << std::endl;
  // Parse each message
  for (size_t thread_idx = 0; thread_idx < n_threads; ++thread_idx) {
    size_t startIndex = thread_idx * chunkSize;
    size_t endIndex = (thread_idx == n_threads - 1) ? numRows : (thread_idx + 1) * chunkSize;
    // const auto& maps = ParseToEcmMap(data_ptrs[j].getData(), data_ptrs[j].getSize(), delim);
    threads.emplace_back([this, &n,&mux, &messages, &data, thread_idx, startIndex, endIndex, numRows, delim](){ /*&data_ptrs*/
      for(size_t j = startIndex; j < endIndex; ++j){
        const auto& maps = ParseToEcmMap(data[j].getData(), data[j].getSize(), delim);
        messages[thread_idx].insert(messages[thread_idx].end(), maps.begin(), maps.end());
        {
          std::lock_guard<std::mutex> lock(mux);
          std::cout << ++n << "\r" << std::flush;
        }
      }
    });
  }
  for (auto& thread : threads) {
    thread.join();
  }
  std::cout << std::endl;
  std::cout << "Done loading chunk" << std::endl;
  //map of keys to timestamp and instance id fields for each message
  std::unordered_map<std::string, std::string> message_to_timestamp;
  std::unordered_map<std::string, std::string> message_to_instance_id;
  // Once I have each message parsed, write the data to plotjuggler
  for(const auto& message_thread : messages){
    for(const auto& map : message_thread){
      // One message is an EcmMessageMap

      // Find the type of the message
      std::string first_key = map.begin()->first;
      size_t pos = first_key.find(delim);
      std::string message_type = map.begin()->first;
      std::string instance_id = "";
      if (pos != std::string::npos){
        message_type = message_type.substr(0,pos);
      }
      // Find the key that points to the instance id of this message type
      if(message_to_instance_id.find(message_type) == message_to_instance_id.end()){
        std::string instance_key;
        const std::string instance_suffix = "component" + delim + "instance"; 
        for(auto it = map.begin(); it!= map.end(); ++it){
          const std::string& key = it->first;
          if(key.size() > instance_suffix.size() && key.compare(key.size() - instance_suffix.size(), instance_suffix.size(), instance_suffix) == 0 ){
            message_to_instance_id.insert({message_type, key});
          }
        }
      }
      // Find the key that points to the timestamp of this message type
      if(message_to_timestamp.find(message_type) == message_to_timestamp.end()){
        std::string timestamp_key;
        const std::string timestamp_suffix = "BusObject" + delim + "write_timestamp_ns";
        for(auto it = map.begin(); it!= map.end(); ++it){
          const std::string& key = it->first;
          if(key.size() > timestamp_suffix.size() && key.compare(key.size() - timestamp_suffix.size(), timestamp_suffix.size(), timestamp_suffix) == 0 ){
            message_to_timestamp.insert({message_type, key});
          }
        }
      }
      // Get the timestamp and instance id
      const auto& timestamp_key = message_to_timestamp.at(message_type);
      const double timestamp = std::get<double>(map.at(timestamp_key)) / 1e9;
      if(message_to_instance_id.find(message_type) != message_to_instance_id.end()){
        const auto& instance_key = message_to_instance_id.at(message_type);
        instance_id = "_"+  std::to_string(static_cast<size_t>(std::get<double>(map.at(instance_key))));
      }
      // For each entry in the map, add it to plotjuggler
      for (const auto& pair: map){
        std::string field_name_str = pair.first;
        if(instance_id.size() > 0)
          field_name_str = field_name_str.insert(field_name_str.find(delim), "_"+instance_id);
        QString field_name = QString::fromStdString(field_name_str);
        WriteToPlotjugglerThreadSafe(field_name, pair.second, timestamp);
      }
    }
  }

  return true;
}

bool ElroyLogLoader::readDataFromFile_multithread(PJ::FileLoadInfo* fileload_info,
                      PlotDataMapRef& plot_data){
  auto startTime = std::chrono::high_resolution_clock::now();
  int numThreads = 8;
  std::vector<std::thread> threads;
  std::string delim = "/";
  _plot_data = &plot_data;  
  
  // Initialize pointers to plotjuggler plots
  std::map<QString, PlotData*> plots_map;
  // Initialize pointers to plotjuggler string plots (this only displays the strings in the tree and does not plot them)
  std::map<QString, PJ::StringSeries*> string_map;

  // Load the elroy_log database file into a vector of packets
  const auto& path = fileload_info->filename.toStdString();
  sqlite3* db;
  int rc = sqlite3_open(path.c_str(), &db);
  if (rc != SQLITE_OK) {
    std::cerr << "Cannot open database: " << sqlite3_errmsg(db) << std::endl;
    return rc;
  }
  // Count rows
  const char* row_query = "SELECT COUNT(*) FROM records;";
  sqlite3_stmt* stmt;
  rc = sqlite3_prepare_v2(db, row_query, -1, &stmt, nullptr);
  if (rc != SQLITE_OK) {
      std::cerr << "Cannot prepare query: " << sqlite3_errmsg(db) << std::endl;
      sqlite3_close(db);
      return rc;
  }
  // Execute the query
  rc = sqlite3_step(stmt);
  if (rc != SQLITE_ROW) {
      std::cerr << "Error executing query: " << sqlite3_errmsg(db) << std::endl;
      sqlite3_finalize(stmt);
      sqlite3_close(db);
      return rc;
  }
  // Get the result of the COUNT(*) query
  int numRows = sqlite3_column_int(stmt, 0);

  // Prepare the SQL query
  const char* query = "SELECT * FROM records;";
  rc = sqlite3_prepare_v2(db, query, -1, &stmt, nullptr);
  if (rc != SQLITE_OK) {
        std::cerr << "Cannot prepare query: " << sqlite3_errmsg(db) << std::endl;
        sqlite3_close(db);
        return rc;
  }        
  size_t count = 0;
  std::vector<BlobData> data_ptrs;
  std::vector<const uint8_t*> raw_data_ptrs;
  std::vector<size_t> data_lens;
  std::cout << "Loading database..." << std::endl;
  while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
      std::cout << count << " of " << numRows << "\r" << std::flush;
      ++count;
      // Access the columns of the current row
      int timestamp = sqlite3_column_int(stmt, 0);
      const uint8_t* raw_data = reinterpret_cast<const uint8_t*>(sqlite3_column_blob(stmt, 1));
      int column2 = sqlite3_column_int(stmt, 3);
      const unsigned char* from_ip = sqlite3_column_text(stmt, 5);
      const unsigned char* git_sha = sqlite3_column_text(stmt, 8);
      size_t byte_array_len = sqlite3_column_int(stmt, 3);
      BlobData myBlob(sqlite3_column_blob(stmt, 1), byte_array_len);
      data_ptrs.push_back(std::move(myBlob)); // 2.03 sec to load 1.5 GB
  }   

  // multiprocessing in order
  size_t chunk_size = 10000;
  size_t n_chunks = static_cast<size_t>(std::ceil(static_cast<double>(data_ptrs.size()) / chunk_size));
  for(size_t i = 0; i < n_chunks; ++i){
    size_t start = i * chunk_size;
    size_t end = (i == n_chunks - 1) ? data_ptrs.size() : (i + 1) * chunk_size;
    std::cout << "start "<<start << " end "<<end <<std::endl;
    MultithreadProcess(data_ptrs, start, end);
    break;
  }


  std::cout << std::endl;
  auto endTime1 = std::chrono::high_resolution_clock::now();
  auto duration3 = std::chrono::duration_cast<std::chrono::milliseconds>(endTime1 - startTime);
  std::cout << "Time to write to plotjuggler: " << duration3.count()/1000.0 << " seconds" << std::endl;
  return true;
  //below this, multiprocessing didn't do things in order


  auto copyEndTime = std::chrono::high_resolution_clock::now();
  auto duration1 = std::chrono::duration_cast<std::chrono::milliseconds>(copyEndTime - startTime);
  std::cout << "Time taken to copy data: " << duration1.count()/1000.0 << " seconds" << std::endl;

  // Convert each packet into a vector of Ecm message maps
  const size_t chunkSize = numRows / numThreads;
  std::vector<std::vector<EcmMessageMap>> list_of_maps(numRows);
  std::vector<EcmMessageListMap> list_of_list_maps(numRows);
  std::vector<std::vector<std::pair<std::string, PlotData::Point>>> pj_points_in_order(numRows);

  std::mutex plotjuggler_mutex;  
  std::cout << "Parsing data..." << std::endl;
  for (size_t thread_idx = 0; thread_idx < numThreads; ++thread_idx) {
    size_t startIndex = thread_idx * chunkSize;
    size_t endIndex = (thread_idx == numThreads - 1) ? numRows : (thread_idx + 1) * chunkSize;
    threads.emplace_back([this, &pj_points_in_order, &plotjuggler_mutex, &plots_map, &string_map, &list_of_list_maps, &data_ptrs, &raw_data_ptrs, &data_lens, &list_of_maps, thread_idx, startIndex, endIndex, numRows, delim](){ /*&data_ptrs*/
      //map of keys to timestamp and instance id fields for each message
      std::unordered_map<std::string, std::string> message_to_timestamp;
      std::unordered_map<std::string, std::string> message_to_instance_id;
      // Store a chunk of data to write to plotjuggler at once
      EcmMessageListMap map_of_lists;
      bool dump_to_plotjuggler = false;
      for(size_t j = startIndex; j < endIndex; ++j){
        // ParseEcmToPlotjuggler(raw_data_ptrs[j], data_lens[j], delim);
        const auto& maps = ParseToEcmMap(data_ptrs[j].getData(), data_ptrs[j].getSize(), delim);
        auto& map_list = list_of_list_maps[j];

        for (const auto& map : maps){
          if (map.size() == 0){
            continue;
          }
          // Find the type of the message
          std::string first_key = map.begin()->first;
          size_t pos = first_key.find(delim);
          std::string message_type = map.begin()->first;
          std::string instance_id = "";
          if (pos != std::string::npos){
            message_type = message_type.substr(0,pos);
          }

          // Find the keys that point to the instance id
          if(message_to_instance_id.find(message_type) == message_to_instance_id.end()){
            std::string instance_key;
            const std::string instance_suffix = "component" + delim + "instance"; 
            for(auto it = map.begin(); it!= map.end(); ++it){
              const std::string& key = it->first;
              if(key.size() > instance_suffix.size() && key.compare(key.size() - instance_suffix.size(), instance_suffix.size(), instance_suffix) == 0 ){
                message_to_instance_id.insert({message_type, key});
              }
            }
          }
          // Find the keys that point to the timestamp
          if(message_to_timestamp.find(message_type) == message_to_timestamp.end()){
            std::string timestamp_key;
            const std::string timestamp_suffix = "BusObject" + delim + "write_timestamp_ns";
            for(auto it = map.begin(); it!= map.end(); ++it){
              const std::string& key = it->first;
              if(key.size() > timestamp_suffix.size() && key.compare(key.size() - timestamp_suffix.size(), timestamp_suffix.size(), timestamp_suffix) == 0 ){
                message_to_timestamp.insert({message_type, key});
              }
            }
          }
          // Get the timestamp and instance id
          const auto& timestamp_key = message_to_timestamp.at(message_type);
          const double timestamp = std::get<double>(map.at(timestamp_key)) / 1e9;
          if(message_to_instance_id.find(message_type) != message_to_instance_id.end()){
            const auto& instance_key = message_to_instance_id.at(message_type);
            instance_id = "_"+  std::to_string(static_cast<size_t>(std::get<double>(map.at(instance_key))));
          }

          // std::lock_guard<std::mutex> lock(_plotjuggler_mutex);
          // std::lock_guard<std::mutex> lock(plotjuggler_mutex);
          // Get the instance id so we know the full message name
          // std::string instance_key = "component" + delim + "instance"; 
          // std::string instance_id = "";
          // double instance_component = 0;
          // for(auto it = map.begin(); it!= map.end(); ++it){
          //   const std::string& key = it->first;
          //   if(key.size() > instance_key.size() && key.compare(key.size() - instance_key.size(), instance_key.size(), instance_key) == 0 ){
          //     instance_id = "_"+  std::to_string(static_cast<size_t>(std::get<double>(map.at(key))));
          //   }
          // }
          // // Get the timestamp
          // double timestamp = 0;
          // std::string timestamp_key = "BusObject" + delim + "write_timestamp_ns";
          // for(auto it = map.begin(); it!= map.end(); ++it){
          //   const std::string& key = it->first;
          //   if(key.size() > timestamp_key.size() && key.compare(key.size() - timestamp_key.size(), timestamp_key.size(), timestamp_key) == 0 ){
          //     timestamp = std::get<double>(map.at(key)) / 1e9;
          //   }
          // }
          // Finding timestamp and instance takes 
          for (const auto& pair: map){
            std::string field_name_str = pair.first;
            if(instance_id.size() > 0)
              field_name_str = field_name_str.insert(field_name_str.find(delim), "_"+instance_id);
            QString field_name = QString::fromStdString(field_name_str);
            // if (map_list.find(field_name_str) == map_list.end()){
            //   map_list.insert({field_name_str,{}});
            //   // std::cout << field_name_str << std::endl;
            //   // if (j == 0)
            //     // std::cout << "\nMap keys:" <<  map_list.size() << std::endl;
            // }
            // else{
            //   std::cout << "duplicate field: " << field_name_str << std::endl;
            // }
            // map_list[field_name_str].push_back(pair.second);
            // WriteToPlotjugglerThreadSafe(field_name, pair.second, timestamp);

            // Add each data to a map that contains a list of map points, which will be periodically cleared and written to plotjuggler
            map_of_lists.insert({field_name_str, {}}); 
            map_of_lists[field_name_str].push_back(pair.second);
            if (map_of_lists[field_name_str].size() > 1000)
              dump_to_plotjuggler = true;
            
            // Problem: writing to plotjuggler using PushBack is slow b/c i'm not doing it in order. Solution:
            // Write to a vector of PlotData points instead
            // if(std::holds_alternative<double>(pair.second)){
            //   PlotData::Point point(timestamp, std::get<double>(pair.second));
            //   pj_points_in_order[thread_idx].push_back({field_name_str, point});
            // }
          }
        }


        // list_of_maps[j] = maps;
        // ParseEcmToPlotjuggler(data_ptrs[j].getData(), data_ptrs[j].getSize(), delim); // 64 sec to load 1.5 GB (without writing to plotjuggler)
        {
          std::lock_guard<std::mutex> lock(this->_counter_mutex);
          this->_msg_counter ++;
          std::cout << "\r" << this->_msg_counter << " of " << numRows<< " " << 100 * this->_msg_counter / numRows << "%" << std::flush;
          if (this->_msg_counter > 10000)
            break;
        }

        // Send everything to plotjuggler - this is slow, perhaps because it is not in order
        if(dump_to_plotjuggler){
          std::lock_guard<std::mutex> lock(this->_counter_mutex);
          for(auto& pair: map_of_lists){
            const auto& field_name = pair.first;
            const auto& data_vec = pair.second;
            // Find the timestamp for this series
            std::string message_type = field_name;
            size_t pos = field_name.find(delim);
            if (pos != std::string::npos){
              message_type = message_type.substr(0,pos);
            }
            if (message_to_timestamp.find(message_type) == message_to_timestamp.end()){
              // Find something that ends in "timestamp_ns" and starts with "message_type"
              const std::string timestamp_suffix = "BusObject" + delim + "write_timestamp_ns";
              const std::string prefix = message_type;
              for(auto it = map_of_lists.begin(); it!= map_of_lists.end(); ++it){
                const std::string& key = it->first;
                if(key.substr(0,prefix.size()) == prefix && key.size() > timestamp_suffix.size() && key.compare(key.size() - timestamp_suffix.size(), timestamp_suffix.size(), timestamp_suffix) == 0 ){
                  message_to_timestamp.insert({message_type, key});
                  break;
                }
              }
            }
            const auto& timestamp_key = message_to_timestamp.at(message_type);
            // WriteToPlotjugglerThreadSafe(field_name, pair.second, timestamp);
            QString q_field_name = QString::fromStdString(field_name);
            {
              std::lock_guard<std::mutex> lock(_plotjuggler_mutex);
              //Add a new column to the plotter if it does not already exist
              if (_plots_map.find(q_field_name) == _plots_map.end() && (std::holds_alternative<double>(data_vec[0]) || std::holds_alternative<bool>(data_vec[0]))){
                auto it = _plot_data->addNumeric(q_field_name.toStdString());
                _plots_map[q_field_name] = (&(it->second));
              }
              else if (_string_map.find(q_field_name) == _string_map.end() && std::holds_alternative<std::string>(data_vec[0]) ){
                auto it = _plot_data->addStringSeries(q_field_name.toStdString());
                _string_map[q_field_name] = (&(it->second));
              }
              // Add the data to the plotter
              if(std::holds_alternative<double>(data_vec[0])){
                for(size_t i = 0; i < data_vec.size(); ++i){
                  const double timestamp = std::get<double>(map_of_lists.at(timestamp_key)[i]) / 1e9;
                  PlotData::Point point(timestamp, std::get<double>(data_vec[i]));
                  // _plots_map[q_field_name]->insert()
                  _plots_map[q_field_name]->pushBack(point);
                }
              }else if(std::holds_alternative<std::string>(data_vec[0])){
                for(size_t i = 0; i < data_vec.size(); ++i){
                  const double timestamp = std::get<double>(map_of_lists.at(timestamp_key)[i]) / 1e9;
                  // _string_map[q_field_name]->pushBack({timestamp, std::get<std::string>(data_vec[i])});
                }
              }
            }
          }
          // Delete the contents of map_of_lists
          map_of_lists.clear();
          dump_to_plotjuggler = false;
        }

      }
    });
  }
  std::cout << std::endl;
  // Wait for all threads to finish
  for (auto& thread : threads) {
      thread.join();
  }

  auto parseEndTime = std::chrono::high_resolution_clock::now();
  auto duration2 = std::chrono::duration_cast<std::chrono::milliseconds>(parseEndTime - copyEndTime);
  std::cout << "Time taken to parse data: " << duration2.count()/1000.0 << " seconds" << std::endl;
  // for(size_t i = 0; i < data_ptrs.size(); ++i){
  //     ParseEcmToPlotjuggler(reinterpret_cast<const uint8_t*>(data_ptrs[i].getData()), data_ptrs[i].getSize(), delim);
  //     std::cout << i << std::endl;
  //     if(i > 10000)
  //       break;
  // }
  sqlite3_finalize(stmt);
  sqlite3_close(db);
  count = 0;
  std::cout << "Writing to plotjuggler..." << std::endl;
  for(const auto& vec_of_maps: list_of_maps){
    for(const auto& map : vec_of_maps){
      std::cout << ++count << " " << numRows<< "\r" << std::flush;
      // Get the instance id
      std::string instance_key = "component" + delim + "instance"; 
      std::string instance_id = "";
      double instance_component = 0;
      for(auto it = map.begin(); it!= map.end(); ++it){
        const std::string& key = it->first;
        if(key.size() > instance_key.size() && key.compare(key.size() - instance_key.size(), instance_key.size(), instance_key) == 0 ){
          instance_id = "_"+  std::to_string(static_cast<size_t>(std::get<double>(map.at(key))));
        }
      }
      // Get the timestamp
      double timestamp = 0;
      std::string timestamp_key = "BusObject" + delim + "write_timestamp_ns";
      for(auto it = map.begin(); it!= map.end(); ++it){
        const std::string& key = it->first;
        if(key.size() > timestamp_key.size() && key.compare(key.size() - timestamp_key.size(), timestamp_key.size(), timestamp_key) == 0 ){
          timestamp = std::get<double>(map.at(key)) / 1e9;
        }
      }
      for (const auto& pair: map){
        std::string field_name_str = pair.first;
        field_name_str = field_name_str.insert(field_name_str.find(delim), "_"+instance_id);
        QString field_name = QString::fromStdString(field_name_str);
        WriteToPlotjugglerThreadSafe(field_name, pair.second, timestamp);
      }
    }
  }
  std::cout << std::endl;
  auto endTime = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - parseEndTime);
  std::cout << "Time to write to plotjuggler: " << duration.count()/1000.0 << " seconds" << std::endl;
  return true;
}
// bool ElroyLogLoader::readDataFromFile_multithread(PJ::FileLoadInfo* fileload_info,
//                       PlotDataMapRef& plot_data){
//   // Read in multithread mode from the database                      
//   auto startTime = std::chrono::high_resolution_clock::now();
//   int numThreads = 8;
//   int chunk_size = 100;
//   std::vector<std::thread> threads;
//   std::string delim = "/";
//   _plot_data = &plot_data;  
  
//   // Initialize pointers to plotjuggler plots
//   std::map<QString, PlotData*> plots_map;
//   // Initialize pointers to plotjuggler string plots (this only displays the strings in the tree and does not plot them)
//   std::map<QString, PJ::StringSeries*> string_map;

//   // Load the elroy_log database file into a vector of packets
//   const auto& path = fileload_info->filename.toStdString();
//   sqlite3* db;
//   int rc = sqlite3_open(path.c_str(), &db);
//   if (rc != SQLITE_OK) {
//     std::cerr << "Cannot open database: " << sqlite3_errmsg(db) << std::endl;
//     return rc;
//   }
//   // Count rows
//   const char* row_query = "SELECT COUNT(*) FROM records;";
//   sqlite3_stmt* stmt;
//   rc = sqlite3_prepare_v2(db, row_query, -1, &stmt, nullptr);
//   if (rc != SQLITE_OK) {
//       std::cerr << "Cannot prepare query: " << sqlite3_errmsg(db) << std::endl;
//       sqlite3_close(db);
//       return rc;
//   }
//   // Execute the query
//   rc = sqlite3_step(stmt);
//   if (rc != SQLITE_ROW) {
//       std::cerr << "Error executing query: " << sqlite3_errmsg(db) << std::endl;
//       sqlite3_finalize(stmt);
//       sqlite3_close(db);
//       return rc;
//   }
//   // Get the result of the COUNT(*) query
//   int numRows = sqlite3_column_int(stmt, 0);

//   // Prepare the SQL query
//   const char* query = "SELECT * FROM records;";
//   rc = sqlite3_prepare_v2(db, query, -1, &stmt, nullptr);
//   if (rc != SQLITE_OK) {
//         std::cerr << "Cannot prepare query: " << sqlite3_errmsg(db) << std::endl;
//         sqlite3_close(db);
//         return rc;
//   }           
//   // Execute the query in multithreaded mode
//   size_t rows_per_thread = numRows / numThreads;
//   for (size_t i = 0; i < numThreads; ++i){
//     size_t start_row = i * rows_per_thread;
//     int end_row = (i == numThreads - 1) ? numRows : (i + 1) * rows_per_thread;
//     threads.emplace_back([this,i, &query, start_row, end_row, numRows, path] {
//       // Perform database read operation for the assigned range
//       // A new db instance must be created per thread
//       sqlite3* db;
//       sqlite3_stmt* stmt;
//       // Open the database
//       {
//           std::lock_guard<std::mutex> lock(this->_db_mutex);
//           if (sqlite3_open(path.c_str(), &db) != SQLITE_OK) {
//               std::cerr << "Cannot open database" << std::endl;
//               return;
//           }
//       }
//       // Prepare the query
//       {
//           std::lock_guard<std::mutex> lock(this->_db_mutex);
//           if (sqlite3_prepare_v2(db, query, -1, &stmt, nullptr) != SQLITE_OK) {
//               std::cerr << "Error preparing query: " << sqlite3_errmsg(db) << std::endl;
//               sqlite3_close(db);
//               return;
//           }
//       }
//       // Move to the starting row
//       for (int i = 0; i < start_row; ++i) {
//           std::lock_guard<std::mutex> lock(this->_db_mutex);
//           if (sqlite3_step(stmt) != SQLITE_ROW) {
//               // Handle error or reach the end of the result set
//               break;
//           }
//       }
//       // Read data within the specified range
//       for (int i = start_row; i < end_row; ++i) {
//           {
//             // increment counter
//             std::lock_guard<std::mutex>(this->_counter_mutex);
//             this->_msg_counter ++;
//             if (i % 10 == 0){
//               std::cout << this->_msg_counter << " : " << numRows << std::endl;
//             }
//             if (this->_msg_counter > 10000)
//               break;
//           }
//           std::vector<EcmMessageMap> maps;
//           {
//               std::lock_guard<std::mutex> lock(this->_db_mutex);
//               if (sqlite3_step(stmt) == SQLITE_ROW) {
//                   // Read data from the current row
//                   int timestamp = sqlite3_column_int(stmt, 0);
//                   const uint8_t* raw_data = reinterpret_cast<const uint8_t*>(sqlite3_column_blob(stmt, 1)); // 3.6 seconds to read all from database
//                   int column2 = sqlite3_column_int(stmt, 3);
//                   const unsigned char* from_ip = sqlite3_column_text(stmt, 5);
//                   const unsigned char* git_sha = sqlite3_column_text(stmt, 8);
//                   size_t byte_array_len = sqlite3_column_int(stmt, 3);
//                   // Process the data as needed
//                   maps = this->ParseToEcmMap(raw_data, byte_array_len);
//               } else {
//                   // Handle error or reach the end of the result set
//                   break;
//               }
//           }
//           // Send the maps to plotjuggler
//           for(const auto& map : maps){

//             // this->WriteToPlotjugglerThreadSafe();
//           }
//       }
//     });
//   }
//   // Join all threads
//   for (auto& thread : threads) {
//       thread.join();
//   }
//   auto endTime = std::chrono::high_resolution_clock::now();
//   auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
//   std::cout << "Time taken by function: " << duration.count()/1000.0 << " seconds" << std::endl;
//   return true;
// }
bool ElroyLogLoader::readDataFromFile(PJ::FileLoadInfo* fileload_info,
                      PlotDataMapRef& plot_data){
  return readDataFromFile_multithread(fileload_info, plot_data);                        
  auto startTime = std::chrono::high_resolution_clock::now();
  int numThreads = 8;
  std::string delim = "/";
  _plot_data = &plot_data;

  // Initialize pointers to plotjuggler plots
  std::map<QString, PlotData*> plots_map;
  // Initialize pointers to plotjuggler string plots (this only displays the strings in the tree and does not plot them)
  std::map<QString, PJ::StringSeries*> string_map;

  // Load the elroy_log database file into a vector of packets
  const auto& path = fileload_info->filename.toStdString();
  sqlite3* db;
  int rc = sqlite3_open(path.c_str(), &db);
  if (rc != SQLITE_OK) {
    std::cerr << "Cannot open database: " << sqlite3_errmsg(db) << std::endl;
    return rc;
  }
  // Count rows
  const char* row_query = "SELECT COUNT(*) FROM records;";
  sqlite3_stmt* stmt;
  rc = sqlite3_prepare_v2(db, row_query, -1, &stmt, nullptr);
  if (rc != SQLITE_OK) {
      std::cerr << "Cannot prepare query: " << sqlite3_errmsg(db) << std::endl;
      sqlite3_close(db);
      return rc;
  }
  // Execute the query
  rc = sqlite3_step(stmt);
  if (rc != SQLITE_ROW) {
      std::cerr << "Error executing query: " << sqlite3_errmsg(db) << std::endl;
      sqlite3_finalize(stmt);
      sqlite3_close(db);
      return rc;
  }
  // Get the result of the COUNT(*) query
  int numRows = sqlite3_column_int(stmt, 0);

  // Prepare the SQL query
  const char* query = "SELECT * FROM records;";
  // sqlite3_stmt* stmt;
  rc = sqlite3_prepare_v2(db, query, -1, &stmt, nullptr);
  if (rc != SQLITE_OK) {
        std::cerr << "Cannot prepare query: " << sqlite3_errmsg(db) << std::endl;
        sqlite3_close(db);
        return rc;
  }
  size_t count = 0;
  elroy_common_msg::MsgDecoder decoder;
  std::vector<EcmMessageMap> maps;
  size_t n_msgs = 0;
  // Execute the query and retrieve data
  while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
      std::cout << count << " of " << numRows << std::endl;
      ++count;
      // Access the columns of the current row
      int timestamp = sqlite3_column_int(stmt, 0);
      const uint8_t* raw_data = reinterpret_cast<const uint8_t*>(sqlite3_column_blob(stmt, 1));
      int column2 = sqlite3_column_int(stmt, 3);
      const unsigned char* from_ip = sqlite3_column_text(stmt, 5);
      const unsigned char* git_sha = sqlite3_column_text(stmt, 8);
      size_t byte_array_len = sqlite3_column_int(stmt, 3);
      ParseEcmToPlotjuggler(raw_data, byte_array_len, delim);
      if (count > 10000)
        break;
      continue;


      size_t bytes_processed = 0;
      size_t current_index = 0;
      while (current_index < byte_array_len){
        EcmMessageMap map;
        elroy_common_msg::MessageDecoderResult res;
        if (!decoder.DecodeAsMap(raw_data + current_index , byte_array_len-current_index, bytes_processed, map, res, delim)){
          break;
        }
        current_index += bytes_processed;
        // Find the instance ID and rename
        if (map.size() == 0)
          continue;
        // Get the instance id
        std::string instance_key = "component" + delim + "instance"; 
        std::string instance_id = "";
        double instance_component = 0;
        for(auto it = map.begin(); it!= map.end(); ++it){
          const std::string& key = it->first;
          if(key.size() > instance_key.size() && key.compare(key.size() - instance_key.size(), instance_key.size(), instance_key) == 0 ){
            instance_id = "_"+  std::to_string(static_cast<size_t>(std::get<double>(map.at(key))));
          }
        }
        // Get the timestamp
        double timestamp = 0;
        std::string timestamp_key = "BusObject" + delim + "write_timestamp_ns";
        for(auto it = map.begin(); it!= map.end(); ++it){
          const std::string& key = it->first;
          if(key.size() > timestamp_key.size() && key.compare(key.size() - timestamp_key.size(), timestamp_key.size(), timestamp_key) == 0 ){
            timestamp = std::get<double>(map.at(key)) / 1e9;
          }
        }
        // Write each element from the ecm message map to plotjuggler
        for (const auto& pair: map){
          n_msgs++;
          std::string field_name_str = pair.first;
          field_name_str = field_name_str.insert(field_name_str.find(delim), "_"+instance_id);
          QString field_name = QString::fromStdString(field_name_str);

          // Add a new column to the plotter if it does not already exist
          if ((std::holds_alternative<double>(pair.second) || std::holds_alternative<bool>(pair.second)) && plots_map.find(field_name) == plots_map.end()){
            auto it = plot_data.addNumeric(field_name.toStdString());
            plots_map[field_name] = (&(it->second));
          }
          else if (std::holds_alternative<std::string>(pair.second) && string_map.find(field_name) == string_map.end()){
            auto it = plot_data.addStringSeries(field_name.toStdString());
            string_map[field_name] = (&(it->second));
          }
          // Add the data to the plotter
          if(std::holds_alternative<double>(pair.second)){
            PlotData::Point point(timestamp, std::get<double>(pair.second));
            plots_map[field_name]->pushBack(point);
          }else if(std::holds_alternative<std::string>(pair.second)){
            string_map[field_name]->pushBack({timestamp, std::get<std::string>(pair.second)});
            continue;
          }
        }
        // maps.push_back(map);
      }
  }
  auto endTime = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
  std::cout << "Time taken by function: " << duration.count()/1000.0 << " seconds" << std::endl;
  return true;
};
int main()
{
  ElroyLogLoader loader;
  PJ::FileLoadInfo file_info;
  file_info.filename = QString::fromStdString("/home/mark/Downloads/2023_11_12-14_14-Elroy_Log_File-ELROYLOG_V1_1-65ab13ad_RECOVERED.elroy_log");
  PlotDataMapRef plot_data;
  loader.readDataFromFile(&file_info,plot_data);
}
