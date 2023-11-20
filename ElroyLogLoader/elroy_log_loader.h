#pragma once

// Note: I had to install plotjuggler from source for plugins to work. Error in snap: 
// "libPcapLoader.so" :  "Cannot load library /home/mark/Desktop/libPcapLoader.so: (/lib/x86_64-linux-gnu/libstdc++.so.6: version `GLIBCXX_3.4.29' not found (required by /home/mark/Desktop/libPcapLoader.so))"

// Might need to install these?
// sudo apt-get install gcc-4.9
// sudo apt-get upgrade libstdc++6

#include <QObject>
#include <QtPlugin>
#include "PlotJuggler/dataloader_base.h"

using namespace PJ;

class ElroyLogLoader : public DataLoader
{
  Q_OBJECT
  Q_PLUGIN_METADATA(IID "facontidavide.PlotJuggler3.DataLoader")
  Q_INTERFACES(PJ::DataLoader)

public:
  ElroyLogLoader();
  virtual const std::vector<const char*>& compatibleFileExtensions() const override{
    return _extensions;
  };
  bool readDataFromFile(PJ::FileLoadInfo* fileload_info,
                        PlotDataMapRef& destination) override;
  bool readDataFromFile_multithread(PJ::FileLoadInfo* fileload_info,
                        PlotDataMapRef& destination);                      

  ~ElroyLogLoader() override = default;

  virtual const char* name() const override
  {
    return "Elroy Log";
  };


protected:
  QSize parseHeader(QFile* file, std::vector<std::string>& ordered_names);

private:
  using EcmMessageMap = std::unordered_map<std::string, std::variant<std::string, double, bool>>;
  using EcmMessageListMap = std::unordered_map<std::string, std::vector<std::variant<std::string, double, bool>>>;
  void WriteToPlotjugglerThreadSafe(const QString& field_name, const std::variant<std::string, double, bool> &data, double timestamp);
  bool ParseEcmToPlotjuggler(const uint8_t* const buf, size_t buff_len, const std::string& delim = "/");
  std::vector<EcmMessageMap> ParseToEcmMap(const uint8_t* const buf, size_t buff_len, const std::string& delim = "/");

  std::vector<const char*> _extensions;

  std::string _default_time_axis;

  // Pointers to plotjuggler plots
  std::unordered_map<QString, PlotData*> _plots_map;
  // Pointers pointers to plotjuggler string plots (this only displays the strings in the tree and does not plot them)
  std::unordered_map<QString, PJ::StringSeries*> _string_map;

  // Mutex for writing to plotjuggler
  std::mutex _plotjuggler_mutex;
  PlotDataMapRef* _plot_data;
  std::mutex _db_mutex;

  size_t _msg_counter = 0;
  std::mutex _counter_mutex;
};
