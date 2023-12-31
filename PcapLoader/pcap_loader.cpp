#include "pcap_loader.h"

#include "IPv4Layer.h"
#include "Packet.h"
#include "PcapFileDevice.h"
#include "UdpLayer.h"

#include <QTextStream>
#include <QFile>
#include <QMessageBox>
#include <QDebug>
#include <QSettings>
#include <QProgressDialog>
#include <QDateTime>
#include <QInputDialog>
#include <thread>
// #include <algorithm>
// #include <execution>
// #include <valgrind/callgrind.h>


#include <chrono>

#include "elroy_common_msg/msg_handling/msg_decoder.h"

// static constexpr std::vector<type> numeric_types{int, double};
// std::vector<std::reference_wrapper<const std::type_info>> numericTypes = {
//     typeid(int),
//     // typeid(float),
//     // typeid(double)
//     // Add more numeric types as needed
// };

using EcmMessageMap = std::unordered_map<std::string, std::variant<std::string, double, bool>>;

PcapLoader::PcapLoader(){
    _extensions.push_back("pcap");
}

std::unordered_map<std::string, std::vector<std::variant<std::string, double, bool>>> PcapLoader::ProcessPackets(std::vector<pcpp::RawPacket> &vec, size_t start_idx, size_t end_idx)const{
  std::unordered_map<std::string, std::vector<std::variant<std::string, double, bool>>> map_of_vec;
  elroy_common_msg::MsgDecoder decoder;
  for (size_t i = start_idx; i < end_idx; ++i){
    size_t current_index = 0;
    pcpp::Packet parsed_packet(&vec[i]);
    const auto& ipv4_layer = parsed_packet.getLayerOfType<pcpp::IPv4Layer>();
    const auto& udp_layer = parsed_packet.getLayerOfType<pcpp::UdpLayer>();
    const auto& byte_array = udp_layer->getLayerPayload();
    size_t byte_array_len = udp_layer->getLayerPayloadSize();
    size_t bytes_processed = 0;
    std::string delim = "/";
    elroy_common_msg::MessageDecoderResult res;
    while (current_index < byte_array_len){
      if (!decoder.DecodeAsMapOfArrays(byte_array + current_index , byte_array_len-current_index, bytes_processed, map_of_vec, res, delim))
        break;
      current_index += bytes_processed;
    }
  }
  return map_of_vec;
}
std::vector<EcmMessageMap> PcapLoader::ParseOnePacketToMap(pcpp::RawPacket &packet, const std::string &delim)const{
  //elroy_common_msg::MsgDecoder decoder;
  std::vector<EcmMessageMap> maps;
  size_t current_index = 0;
  pcpp::Packet parsed_packet(&packet);
  const auto& ipv4_layer = parsed_packet.getLayerOfType<pcpp::IPv4Layer>();
  const auto& udp_layer = parsed_packet.getLayerOfType<pcpp::UdpLayer>();
  const auto& byte_array = udp_layer->getLayerPayload();
  size_t byte_array_len = udp_layer->getLayerPayloadSize();
  size_t bytes_processed = 0;
  while (current_index < byte_array_len){
    std::unordered_map<std::string, std::variant<std::string, double, bool>> map;
    elroy_common_msg::MessageDecoderResult res;
    if (!elroy_common_msg::MsgDecoder::DecodeAsMap(byte_array + current_index , byte_array_len-current_index, bytes_processed, map, res, delim)){
      break;
    }
    current_index += bytes_processed;
    maps.push_back(map);
  }
  return maps;
}
bool PcapLoader::readDataFromFile_transform(PJ::FileLoadInfo* fileload_info, PlotDataMapRef& plot_data){

  auto startTime = std::chrono::high_resolution_clock::now();
  int numThreads = 8;
  std::vector<pcpp::RawPacket> packet_data;
  std::vector<std::unordered_map<std::string, std::variant<std::string, double, bool>>> vec_of_map_of_variants;

  // Load the pcap file
  const auto& path = fileload_info->filename.toStdString();
  std::cout <<"File name: " <<  path << std::endl;
  pcpp::PcapFileReaderDevice reader(path);
  if (!reader.open()) {
    std::string m = "Could not open pcap file: {}"+ path;
    throw std::runtime_error(m);
  }
  pcpp::RawPacket raw_packet;
  while (reader.getNextPacket(raw_packet)){
    packet_data.push_back(raw_packet);
  }
//   std::transform(
//     std::execution::par,            // Parallel execution policy
//     packet_data.begin(), packet_data.end(), // Input range
//     vec_of_map_of_variants.begin(),              // Output range
//     [](pcpp::RawPacket packet) {
//         // process packets one at a time
//         return ParseOnePacketToMap(packet);
//     }
// );
return true;
}

bool PcapLoader::readDataFromFile_mulithread(PJ::FileLoadInfo* fileload_info, PlotDataMapRef& plot_data){
  // Warning: This function uses tons of memory!
  auto startTime = std::chrono::high_resolution_clock::now();
  int numThreads = 1;
  std::string delim = "/";

  // Initialize pointers to plotjuggler plots
  std::map<QString, PlotData*> plots_map;
  // Initialize pointers to plotjuggler string plots (this only displays the strings in the tree and does not plot them)
  std::map<QString, PJ::StringSeries*> string_map;

  // Load the pcap file into a vector of packets
  std::vector<pcpp::RawPacket> packet_data;
  const auto& path = fileload_info->filename.toStdString();
  pcpp::PcapFileReaderDevice reader(path);
  if (!reader.open()) {
    std::string m = "Could not open pcap file: {}"+ path;
    throw std::runtime_error(m);
  }
  pcpp::RawPacket raw_packet;
  while (reader.getNextPacket(raw_packet)){
    packet_data.push_back(raw_packet);
  }

  // Convert each packet into a vector of Ecm message maps
  std::vector<std::vector<EcmMessageMap>> vec_of_maps(numThreads);
  const size_t chunkSize = packet_data.size() / numThreads;
  std::vector<std::thread> threads;
  for (size_t thread_idx = 0; thread_idx < numThreads; ++thread_idx) {
    size_t startIndex = thread_idx * chunkSize;
    size_t endIndex = (thread_idx == numThreads - 1) ? packet_data.size() : (thread_idx + 1) * chunkSize;
    threads.emplace_back([this, &packet_data, &vec_of_maps, thread_idx, startIndex, endIndex](){
      for(size_t j = startIndex; j < endIndex; ++j){
        const auto& message_maps = this->ParseOnePacketToMap(packet_data[j]);
        vec_of_maps[thread_idx].insert(vec_of_maps[thread_idx].end(), message_maps.begin(), message_maps.end());
      }
    });
  }
  // Wait for all threads to finish
  for (auto& thread : threads) {
      thread.join();
  }
  size_t n_msgs = 0;
  // Add each ecm message map to plotjuggler
  for (const auto& vec_per_thread : vec_of_maps){
    for(const auto& map : vec_per_thread){
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
    }
  }
  auto endTime = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
  std::cout << "Time taken by function: " << duration.count()/1000.0 << " seconds" << std::endl; // takes 5sec for 93mb
  std::cout << "N_msgs = " << n_msgs << std::endl;
  return true;
}

bool PcapLoader::readDataFromFile_mulithread_old(PJ::FileLoadInfo* fileload_info, PlotDataMapRef& plot_data){
  auto startTime = std::chrono::high_resolution_clock::now();
  int numThreads = 8;
  std::vector<pcpp::RawPacket> packet_data;
  std::vector<std::unordered_map<std::string, std::vector<std::variant<std::string, double, bool>>>> vec_of_map_of_vec(numThreads);

  // Load the pcap file
  const auto& path = fileload_info->filename.toStdString();
  std::cout <<"File name: " <<  path << std::endl;
  // const bool file_exists = access(fileload_info->filename, 0) == 0;
  pcpp::PcapFileReaderDevice reader(path);
  if (!reader.open()) {
    std::string m = "Could not open pcap file: {}"+ path;
    throw std::runtime_error(m);
  }
  pcpp::RawPacket raw_packet;
  while (reader.getNextPacket(raw_packet)){
    packet_data.push_back(raw_packet);
  }
  std::vector<std::thread> threads;
  const size_t chunkSize = packet_data.size() / numThreads;
  for (size_t i = 0; i < numThreads; ++i) {
    size_t startIndex = i * chunkSize;
    size_t endIndex = (i == numThreads - 1) ? packet_data.size() : (i + 1) * chunkSize;
    threads.emplace_back([this, &packet_data, &vec_of_map_of_vec, i, startIndex, endIndex](){
      vec_of_map_of_vec[i] = this->ProcessPackets(packet_data, startIndex, endIndex);
    });
  }
  // Wait for all threads to finish
  for (auto& thread : threads) {
      thread.join();
  }

  // Join together the maps of arrays
  std::unordered_map<std::string, std::vector<std::variant<std::string, double, bool>>> map_of_vecs;
  for (const auto& map : vec_of_map_of_vec){
    for (const auto& key_val : map){
      const auto& key = key_val.first;
      map_of_vecs.insert({key, {}});
      // std::cout << key << " has n messages: " << key_val.second.size() << std::endl;
      map_of_vecs[key].insert(map_of_vecs[key].end(), key_val.second.begin(), key_val.second.end());
      continue;
    }
  }

  // Now that the map of vectors has been populated, for each key in that map copy the vector to plotjuggler

  // We need to record pointers to each plot
  size_t n_msgs = 0;
  std::map<QString, PlotData*> plots_map;
  std::map<QString, PJ::StringSeries*> string_map;
  if(map_of_vecs.size() > 0 && true){
    for (const auto& pair: map_of_vecs){
      QString field_name = QString::fromStdString(pair.first);
      if(pair.second.size() == 0)
        continue;
      n_msgs+= pair.second.size();
      // Add this field name to plotjuggler if it doesn't exist
      if (std::holds_alternative<double>(pair.second[0]) && plots_map.find(field_name) == plots_map.end()){
        auto it = plot_data.addNumeric(pair.first);
        plots_map[field_name] = (&(it->second));
      }
      else if (std::holds_alternative<std::string>(pair.second[0]) && plots_map.find(field_name) == plots_map.end()){
        auto it = plot_data.addStringSeries(pair.first);
        string_map[field_name] = (&(it->second));
      }
      size_t i = 0;
      for(const auto& value : pair.second){
        ++i;
        if(std::holds_alternative<double>(value)){
            // Get the timestamp from the udp packet. I might need to use the udp layer
            PlotData::Point point(i, std::get<double>(value));
            plots_map[field_name]->pushBack(point);
          }else if(std::holds_alternative<std::string>(value)){
            // std::cout << std::get<std::string>(pair.second) << std::endl;
            string_map[field_name]->pushBack({static_cast<double>(i), std::get<std::string>(value)});
            continue;
          }
      }
    }
  }else{
    std::cout << "map of vec is empty" << std::endl;
  }
  auto endTime = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
  std::cout << "Time taken by function: " << duration.count()/1000.0 << " seconds" << std::endl;
  std::cout << "N_msgs = " << n_msgs << std::endl; //9743346
  return true;
}
bool PcapLoader::readDataFromFile(PJ::FileLoadInfo* fileload_info,
                        PlotDataMapRef& plot_data){
  return PcapLoader::readDataFromFile_mulithread_old(fileload_info, plot_data);                        
  // return PcapLoader::readDataFromFile_mulithread(fileload_info, plot_data);                        

  auto startTime = std::chrono::high_resolution_clock::now();
  std::string delim = "/";

  // Load the pcap file
  const auto& path = fileload_info->filename.toStdString();
  std::cout <<"File name: " <<  path << std::endl;
  // const bool file_exists = access(fileload_info->filename, 0) == 0;
  pcpp::PcapFileReaderDevice reader(path);
  if (!reader.open()) {
    std::string m = "Could not open pcap file: {}"+ path;
    throw std::runtime_error(m);
  }
  pcpp::RawPacket raw_packet; 
  int i = 0;
  elroy_common_msg::MsgDecoder decoder;

  // We need to record pointers to each plot
  std::map<QString, PlotData*> plots_map;
  std::map<QString, PJ::StringSeries*> string_map;

  // Schema for ip addresses
  // std::map<std::string, std::string> ip_src_addr_to_folder_name;
  // ip_src_addr_to_folder_name["172.16.17.11"] = "MfcA";
  std::unordered_map<std::string, std::vector<std::variant<std::string, double, bool>>> map_of_vec;

  while (reader.getNextPacket(raw_packet)){
    auto maps = ParseOnePacketToMap(raw_packet);
    for (auto& map : maps){
      if(map.size() > 0){
        std::string instance_key = "component" + delim + "instance"; 
        std::string instance_id = "";
        double instance_component = 0;
        // Search for the instance ID string
        for(auto it = map.begin(); it!= map.end(); ++it){
          const std::string& key = it->first;
          if(key.size() > instance_key.size() && key.compare(key.size() - instance_key.size(), instance_key.size(), instance_key) == 0 ){
            instance_id = "_"+  std::to_string(static_cast<size_t>(std::get<double>(map[key])));
          }
        }
        // Search for the timestamp
        double timestamp = 0;
        std::string timestamp_key = "BusObject" + delim + "write_timestamp_ns";
        for(auto it = map.begin(); it!= map.end(); ++it){
          const std::string& key = it->first;
          if(key.size() > timestamp_key.size() && key.compare(key.size() - timestamp_key.size(), timestamp_key.size(), timestamp_key) == 0 ){
            timestamp = std::get<double>(map[key]) / 1e9;
          }
        }
        for (const auto& pair: map){
          std::string field_name_str = pair.first;
          field_name_str = field_name_str.insert(field_name_str.find(delim), "_"+instance_id);
          // QString field_name = QString::fromStdString(pair.first);
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
          // std::cout << pair.first << std::endl;
          if(std::holds_alternative<double>(pair.second)){
            PlotData::Point point(timestamp, std::get<double>(pair.second));
            plots_map[field_name]->pushBack(point);
          }else if(std::holds_alternative<std::string>(pair.second)){
            string_map[field_name]->pushBack({static_cast<double>(i), std::get<std::string>(pair.second)});
            continue;
          }
        }
      }
    }
    
    continue;

    size_t current_index = 0;
    i++;
    // std::cout << "Parsing packet " << i << std::endl;
    pcpp::Packet parsed_packet(&raw_packet);
    const auto& ipv4_layer = parsed_packet.getLayerOfType<pcpp::IPv4Layer>();
    const auto& udp_layer = parsed_packet.getLayerOfType<pcpp::UdpLayer>();
    const auto& byte_array = udp_layer->getLayerPayload();
    size_t byte_array_len = udp_layer->getLayerPayloadSize();
    size_t bytes_processed = 0;

    while (current_index < byte_array_len){
      std::unordered_map<std::string, std::variant<std::string, double, bool>> map;
      elroy_common_msg::MessageDecoderResult res;
      std::string delim = "/";

      // if (!decoder.DecodeAsMapOfArrays(byte_array + current_index , byte_array_len-current_index, bytes_processed, map_of_vec, res, delim))
      //   break;
      // current_index += bytes_processed;
      // continue;
      //decode as map without going to plotjuggler: 4.1s
      //decode as map of arrays without going to plotjuggler: 0.11s -> WRONG this wasn't parsing anything. It actually takes longer
      if (!decoder.DecodeAsMap(byte_array + current_index , byte_array_len-current_index, bytes_processed, map, res, delim))
        break;
      current_index += bytes_processed;
      if(map.size() > 0){
        // Find the timestamp, in BusObject/write_timestamp_ns
        double timestamp = 0;
        std::string timestamp_key = "BusObject" + delim + "write_timestamp_ns";
        for(auto it = map.begin(); it!= map.end(); ++it){
          const std::string& key = it->first;
          if(key.size() > timestamp_key.size() && key.compare(key.size() - timestamp_key.size(), timestamp_key.size(), timestamp_key) == 0 ){
            timestamp = std::get<double>(map[key]) / 1e9;
          }
        }
        std::string instance_key = "component" + delim + "instance"; //"instance_str";
        std::string instance_id = "";
        double instance_component = 0;
        for(auto it = map.begin(); it!= map.end(); ++it){
          const std::string& key = it->first;
          if(key.size() > instance_key.size() && key.compare(key.size() - instance_key.size(), instance_key.size(), instance_key) == 0 ){
            // instance_id = "_"+  std::get<std::string>(map[key]);
            instance_id = "_"+  std::to_string(static_cast<size_t>(std::get<double>(map[key])));
          }
        }
        for (const auto& pair: map){
          std::string field_name_str = pair.first;
          field_name_str = field_name_str.insert(field_name_str.find(delim), "_"+instance_id);
          // QString field_name = QString::fromStdString(pair.first);
          QString field_name = QString::fromStdString(field_name_str);
          // if (pair.first.find("VlThrusterState") != std::string::npos){
          //   std::cout << pair.first << std::endl;
          // }

          // Add a new column to the plotter if it does not already exist
          if ((std::holds_alternative<double>(pair.second) || std::holds_alternative<bool>(pair.second)) && plots_map.find(field_name) == plots_map.end()){
            auto it = plot_data.addNumeric(field_name.toStdString());
            plots_map[field_name] = (&(it->second));
          }
          else if (std::holds_alternative<std::string>(pair.second) && string_map.find(field_name) == string_map.end()){
            auto it = plot_data.addStringSeries(field_name.toStdString());
            string_map[field_name] = (&(it->second));
          }
          // std::cout << pair.first << std::endl;
          if(std::holds_alternative<double>(pair.second)){
            // std::cout << std::get<double>(pair.second) << std::endl;
            // Get the timestamp from the udp packet. I might need to use the udp layer
            PlotData::Point point(timestamp, std::get<double>(pair.second));
            plots_map[field_name]->pushBack(point);
          }else if(std::holds_alternative<std::string>(pair.second)){
            // std::cout << std::get<std::string>(pair.second) << std::endl;
            string_map[field_name]->pushBack({static_cast<double>(i), std::get<std::string>(pair.second)});
            continue;
          }
        }
        // break;
      }
    }
    // CALLGRIND_TOGGLE_COLLECT;
    // CALLGRIND_STOP_INSTRUMENTATION;
  }

  // Now that the map of vectors has been populated, for each key in that map copy the vector to plotjuggler
  if(map_of_vec.size() > 0 && true){
    for (const auto& pair: map_of_vec){
      QString field_name = QString::fromStdString(pair.first);
      if(pair.second.size() == 0)
        continue;
      // Add this field name to plotjuggler if it doesn't exist
      if (std::holds_alternative<double>(pair.second[0]) && plots_map.find(field_name) == plots_map.end()){
        auto it = plot_data.addNumeric(pair.first);
        plots_map[field_name] = (&(it->second));
      }
      else if (std::holds_alternative<std::string>(pair.second[0]) && plots_map.find(field_name) == plots_map.end()){
        auto it = plot_data.addStringSeries(pair.first);
        string_map[field_name] = (&(it->second));
      }
      size_t i = 0;
      for(const auto& value : pair.second){
        ++i;
        if(std::holds_alternative<double>(value)){
            // Get the timestamp from the udp packet. I might need to use the udp layer
            PlotData::Point point(i, std::get<double>(value));
            plots_map[field_name]->pushBack(point);
          }else if(std::holds_alternative<std::string>(value)){
            // std::cout << std::get<std::string>(pair.second) << std::endl;
            string_map[field_name]->pushBack({static_cast<double>(i), std::get<std::string>(value)});
            continue;
          }
      }
    }
  }else{
    std::cout << "map of vec is empty" << std::endl;
  }
  auto endTime = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
  std::cout << "Time taken by function: " << duration.count()/1000.0 << " seconds" << std::endl;
  return true;                         
}

// This is an example program that I'm using for development/debugging. Change the 
// file_info.filename to a pcap file on your computer and run ~/build/PcapLoaderExec
int main()
{
  PcapLoader pcap;
  PJ::FileLoadInfo file_info;
  file_info.filename = QString::fromStdString("/home/mark/code2/fast_ecm_example.pcap");
  PlotDataMapRef plot_data;
  pcap.readDataFromFile(&file_info,plot_data);
}
