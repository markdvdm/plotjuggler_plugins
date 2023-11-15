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

bool PcapLoader::readDataFromFile(PJ::FileLoadInfo* fileload_info,
                        PlotDataMapRef& plot_data){
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
  // size_t chunk_size = 128 * 1024;
  // size_t packets_to_load = 1 * chunk_size_;
  // while (reader_->isOpened()){
    // pcpp::RawPacketVector raw_packets;
    // const size_t loaded_amount = reader_->getNextPackets(raw_packets, packets_to_load);
    // pcpp::RawPacket raw_packet; 
  // }
  pcpp::RawPacket raw_packet; 
  int i = 0;
  elroy_common_msg::MsgDecoder decoder;

  // We need to record pointers to each plot
  std::map<QString, PlotData*> plots_map;

  while (reader.getNextPacket(raw_packet)){
    i++;
    pcpp::Packet parsed_packet(&raw_packet);
    const auto& ipv4_layer = parsed_packet.getLayerOfType<pcpp::IPv4Layer>();
    const auto& udp_layer = parsed_packet.getLayerOfType<pcpp::UdpLayer>();
    const auto& byte_array = udp_layer->getLayerPayload();
    size_t byte_array_len = udp_layer->getLayerPayloadSize();
    size_t bytes_processed = 0;
    std::unordered_map<std::string, std::variant<std::string, double, bool>> map;
    elroy_common_msg::MessageDecoderResult res;
    std::string delim = "/";
    decoder.DecodeAsMap(byte_array, byte_array_len, bytes_processed, map, res, delim);
    if(map.size() > 0){
      // std::cout << "parsing packet " << i << std::endl;
      for (const auto& pair: map){
        QString field_name = QString::fromStdString(pair.first);

        // Add a new column to the plotter if it does not already exist
        if (plots_map.find(field_name) == plots_map.end()){
          auto it = plot_data.addNumeric(pair.first);
          plots_map[field_name] = (&(it->second));
        }

        // std::cout << pair.first << std::endl;
        if(std::holds_alternative<double>(pair.second)){
          // std::cout << std::get<double>(pair.second) << std::endl;
          PlotData::Point point(i, std::get<double>(pair.second));
          plots_map[field_name]->pushBack(point);
        }else if(std::holds_alternative<std::string>(pair.second)){
          // std::cout << std::get<std::string>(pair.second) << std::endl;
          continue;
        }
      }
      // break;

    }
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
