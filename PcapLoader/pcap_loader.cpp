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
#include <algorithm>
// #include <valgrind/callgrind.h>


#include <chrono>

#include "elroy_common_msg/msg_handling/msg_decoder.h"

// static constexpr std::vector<type> numeric_types{int, double};
std::vector<std::reference_wrapper<const std::type_info>> numericTypes = {
    typeid(int),
    // typeid(float),
    // typeid(double)
    // Add more numeric types as needed
};

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
bool PcapLoader::readDataFromFile_mulithread(PJ::FileLoadInfo* fileload_info, PlotDataMapRef& plot_data){
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
      map_of_vecs[key].insert(map_of_vecs[key].end(), key_val.second.begin(), key_val.second.end());
      continue;
    }
  }

  // Now that the map of vectors has been populated, for each key in that map copy the vector to plotjuggler

  // We need to record pointers to each plot
  std::map<QString, PlotData*> plots_map;
  std::map<QString, PJ::StringSeries*> string_map;
  if(map_of_vecs.size() > 0 && true){
    for (const auto& pair: map_of_vecs){
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
bool PcapLoader::readDataFromFile(PJ::FileLoadInfo* fileload_info,
                        PlotDataMapRef& plot_data){
  return PcapLoader::readDataFromFile_mulithread(fileload_info, plot_data);                        

  // CALLGRIND_START_INSTRUMENTATION
  // CALLGRIND_TOGGLE_COLLECT;
  auto startTime = std::chrono::high_resolution_clock::now();

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
  std::map<std::string, std::string> ip_src_addr_to_folder_name;
  ip_src_addr_to_folder_name["172.16.17.11"] = "MfcA";

  std::unordered_map<std::string, std::vector<std::variant<std::string, double, bool>>> map_of_vec;
  while (reader.getNextPacket(raw_packet)){
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
      //decode as map of arrays without going to plotjuggler: 0.11s
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

  // for each msg in the pcap file, call the ecm parser
  // for each message type I need to get the data from it to add to the destination
  //   Can I iterate over an object as if it were a dict? No, C++ does not have reflection
  //   Only solution is to add a GetData() function to the ecm parser that provides a list
  //   of all the data members in a message (and sub-messages). Then I could go over these
  //   and recursively unpack the data so I can add it to the plotjuggler destination. 
  auto endTime = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
  std::cout << "Time taken by function: " << duration.count()/1000.0 << " seconds" << std::endl;
  return true;                         
}

// #include "dataload_simple_csv.h"
// #include <QTextStream>
// #include <QFile>
// #include <QMessageBox>
// #include <QDebug>
// #include <QSettings>
// #include <QProgressDialog>
// #include <QDateTime>
// #include <QInputDialog>

// DataLoadSimpleCSV::DataLoadSimpleCSV()
// {
//   _extensions.push_back("pj_csv");
// }

// const QRegExp csv_separator("(\\,)");

// const std::vector<const char*>& DataLoadSimpleCSV::compatibleFileExtensions() const
// {
//   return _extensions;
// }

// bool DataLoadSimpleCSV::readDataFromFile(FileLoadInfo* info, PlotDataMapRef& plot_data)
// {
//   QFile file(info->filename);
//   if( !file.open(QFile::ReadOnly) )
//   {
//     return false;
//   }
//   QTextStream text_stream(&file);

//   // The first line should contain the name of the columns
//   QString first_line = text_stream.readLine();
//   QStringList column_names = first_line.split(csv_separator);

//   // create a vector of timeseries
//   std::vector<PlotData*> plots_vector;

//   for (unsigned i = 0; i < column_names.size(); i++)
//   {
//     QString column_name = column_names[i].simplified();

//     std::string field_name = column_names[i].toStdString();

//     auto it = plot_data.addNumeric(field_name);

//     plots_vector.push_back(&(it->second));
//   }

//   //-----------------
//   // read the file line by line
//   int linecount = 1;
//   while (!text_stream.atEnd())
//   {
//     QString line = text_stream.readLine();
//     linecount++;

//     // Split using the comma separator.
//     QStringList string_items = line.split(csv_separator);
//     if (string_items.size() != column_names.size())
//     {
//       auto err_msg = QString("The number of values at line %1 is %2,\n"
//                              "but the expected number of columns is %3.\n"
//                              "Aborting...")
//           .arg(linecount)
//           .arg(string_items.size())
//           .arg(column_names.size());

//       QMessageBox::warning(nullptr, "Error reading file", err_msg );
//       return false;
//     }

//     // The first column should contain the timestamp.
//     QString first_item = string_items[0];
//     bool is_number;
//     double t = first_item.toDouble(&is_number);

//     // check if the time format is a DateTime
//     if (!is_number )
//     {
//       QDateTime ts = QDateTime::fromString(string_items[0], "yyyy-MM-dd hh:mm:ss");
//       if (!ts.isValid())
//       {
//           QMessageBox::warning(nullptr, tr("Error reading file"),
//                   tr("Couldn't parse timestamp. Aborting.\n"));

//           return false;
//       }
//       t = ts.toMSecsSinceEpoch()/1000.0;
//     }

//     for (int i = 0; i < string_items.size(); i++)
//     {
//       double y = string_items[i].toDouble(&is_number);
//       if (is_number)
//       {
//         PlotData::Point point(t, y);
//         plots_vector[i]->pushBack(point);
//       }
//     }
//   }

//   file.close();

//   return true;
// }
int main()
{
  PcapLoader pcap;
  PJ::FileLoadInfo file_info;
  file_info.filename = QString::fromStdString("/home/mark/code2/fast_ecm_example.pcap");
  PlotDataMapRef plot_data;
  pcap.readDataFromFile(&file_info,plot_data);
}
