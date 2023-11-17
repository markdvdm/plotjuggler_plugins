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

#include "elroy_common_msg/msg_handling/msg_decoder.h"

using EcmMessageMap = std::unordered_map<std::string, std::variant<std::string, double, bool>>;
ElroyLogLoader::ElroyLogLoader(){
    _extensions.push_back("elroy_log");
}
bool ElroyLogLoader::readDataFromFile(PJ::FileLoadInfo* fileload_info,
                      PlotDataMapRef& plot_data){
  auto startTime = std::chrono::high_resolution_clock::now();
  int numThreads = 8;
  std::string delim = "/";

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
  // Prepare the SQL query
  const char* query = "SELECT * FROM records;";
  sqlite3_stmt* stmt;
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
      ++count;
      // Access the columns of the current row
      int timestamp = sqlite3_column_int(stmt, 0);
      const uint8_t* raw_data = reinterpret_cast<const uint8_t*>(sqlite3_column_blob(stmt, 1));
      int column2 = sqlite3_column_int(stmt, 3);
      const unsigned char* from_ip = sqlite3_column_text(stmt, 5);
      const unsigned char* git_sha = sqlite3_column_text(stmt, 8);

      // Do something with the data
      // std::cout << "Timestamp 1: " << timestamp << ", from_ip: " << from_ip << std::endl;
      // if (count > 50)
      //   break;
      size_t byte_array_len = sqlite3_column_int(stmt, 3);
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
        maps.push_back(map);
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
