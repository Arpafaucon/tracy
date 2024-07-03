#include "TracyEvent.hpp"
#include "TracyFileRead.hpp"
#include "TracyFileWrite.hpp"
#include "TracyWorker.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <iterator>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <unordered_map>
#include <map>
#include <unordered_set>

using namespace std::chrono_literals;


struct ExportedWorker
{
    std::vector<tracy::Worker::ImportEventTimeline> timeline;
    std::unordered_map<uint64_t, std::string> threadNames;

    static ExportedWorker merge(std::vector<ExportedWorker const*> inputList)
    {
        uint64_t nextFreeThreadId = 0;
        ExportedWorker out;
        for(auto exportPtr: inputList)
        {
            ExportedWorker const& exported = *exportPtr;

            // rebuild thread mapping
            std::map<uint64_t, uint64_t> localThreadToMultiprocess;
            for (auto const& threadId : exported.threadNames) {
                uint64_t multiprocessId = nextFreeThreadId++;
                localThreadToMultiprocess[threadId.first] = multiprocessId;
                out.threadNames[multiprocessId] = threadId.second;
            }

            // translate all events with the right thread ID
            for (auto&& event: exported.timeline)
            {
                tracy::Worker::ImportEventTimeline translated{event};
                translated.tid = localThreadToMultiprocess[event.tid];
                out.timeline.emplace_back(translated);
            }
        }
        return out;
    }
};

std::optional<ExportedWorker> exportTracyFile(std::string const& filepath)
{
    std::unique_ptr<tracy::FileRead> sourceFile{tracy::FileRead::Open((filepath.c_str()))};
    if (!sourceFile)
    {
        std::cerr << "Could not find file" << std::endl;
        return std::nullopt;
    }
    std::cout << "reading " << sourceFile << std::endl;

    tracy::Worker worker{*sourceFile,
                         tracy::EventType::All,
                         true,  // otherwise source zones are empty
                         false};
    while (!worker.AreSourceLocationZonesReady())
    {
        std::cout << "Waiting for source locations" << std::endl;
        std::this_thread::sleep_for(700ms);
    }

    ExportedWorker exportedData;

    std::unordered_set<uint16_t> seenThreadIds;

    auto& sourceLocationZones = worker.GetSourceLocationZones();
    for (auto&& zone_it : sourceLocationZones)
    {
        const tracy::SourceLocation& sourceLoc = worker.GetSourceLocation(zone_it.first);
        std::string zoneFilePath                   = worker.GetString(sourceLoc.file);
        int zoneLine                           = sourceLoc.line;
        std::string zoneName = worker.GetString(sourceLoc.name);

        std::cout << "processing source location: " << zoneFilePath << ":" << zoneLine << std::endl;

        auto const& zones = zone_it.second;
        for (auto&& zoneData : zones.zones)
        {
            const auto zone_event = zoneData.Zone();
            const uint16_t tId        = zoneData.Thread();
            const auto start      = zone_event->Start();
            const auto end        = zone_event->End();
            std::cout << "  " << tId << "[]\t" << start << "->" << end << std::endl;
            seenThreadIds.emplace(tId);


            auto& startEvent = exportedData.timeline.emplace_back();
            startEvent.locFile = zoneFilePath;
            startEvent.locLine = zoneLine;
            startEvent.name = zoneName;
            startEvent.tid = tId;
            startEvent.isEnd = false;
            startEvent.timestamp = zone_event->Start();

            auto& endEvent = exportedData.timeline.emplace_back();
            endEvent.locFile = zoneFilePath;
            endEvent.locLine = zoneLine;
            endEvent.name = zoneName;
            endEvent.tid = tId;
            endEvent.isEnd = true;
            endEvent.timestamp = zone_event->End();
        }
    }

    std::cout << "Building thread map" << std::endl;
    for (auto&& tid : seenThreadIds) {
        const uint64_t threadId = worker.DecompressThread(tid);
        std::string name = worker.GetThreadName(threadId);
        exportedData.threadNames[tid] = name;
        std::cout << "resolving thread " << tid << "->" << threadId << "->" << name << std::endl;
    }

    return exportedData;
}

int main()
{
    std::vector<ExportedWorker> exports;
    for(auto path: {"track/0702/test.tracy", "track/0702/test.tracy"})
    {
        auto importedOpt = exportTracyFile("track/0702/test.tracy");
        if (not importedOpt.has_value())
        {
            std::cerr << "empty import" << std::endl;
            return 1;
        }
        exports.push_back((importedOpt.value()));
    }

    std::vector<ExportedWorker const*> exportRefs;
    std::transform(exports.cbegin(), exports.cend(), std::back_inserter(exportRefs), [](ExportedWorker const& ex){return &ex;});

    auto mergedImport = ExportedWorker::merge(exportRefs);

    {
        std::cout << "Writing" << std::endl;
        auto outputFileWrite = std::unique_ptr<tracy::FileWrite> (tracy::FileWrite::Open("out.tracy"));
        if( !outputFileWrite )
        {
            fprintf( stderr, "Cannot open output file!\n" );
            exit( 1 );
        }
        tracy::Worker outputWorker("multi-process-import", "toto", mergedImport.timeline, {}, {}, mergedImport.threadNames);
        outputWorker.Write(*outputFileWrite, false);
    }

    std::cout << "done" << std::endl;

    return 0;
}
