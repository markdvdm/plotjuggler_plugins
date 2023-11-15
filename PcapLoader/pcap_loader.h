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

class PcapLoader : public DataLoader
{
  Q_OBJECT
  Q_PLUGIN_METADATA(IID "facontidavide.PlotJuggler3.DataLoader")
  Q_INTERFACES(PJ::DataLoader)

public:
  PcapLoader();
  virtual const std::vector<const char*>& compatibleFileExtensions() const override{
    return _extensions;
  };

  bool readDataFromFile(PJ::FileLoadInfo* fileload_info,
                        PlotDataMapRef& destination) override;

  ~PcapLoader() override = default;

  virtual const char* name() const override
  {
    return "Elroy PCAP";
  };


protected:
  QSize parseHeader(QFile* file, std::vector<std::string>& ordered_names);

private:
  std::vector<const char*> _extensions;

  std::string _default_time_axis;
};
