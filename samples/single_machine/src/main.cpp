#include <iostream>
#include <filesystem>
#include <vector>
#include <fstream>
#include <sstream>
#include <cstdlib>
#include "../../common/include/common/JobSpec.h"
#include "../../common/include/common/ISplitStrategy.h"
#include "../../common/include/common/FFmpegExecutor.h"
#include "../../common/include/common/TimeBasedSplitStrategy.h"

int main(int argc, char **argv)
{
    if (argc < 4)
    {
        std::cout << "Usage: single_machine_demo <source.mp4> <numChunks> <output.mp4>\n";
        return 1;
    }
    std::string source = argv[1];
    int numChunks = std::stoi(argv[2]);
    std::string outFile = argv[3];

    JobSpec spec;
    spec.jobId = "localjob";
    spec.sourcePath = source;
    spec.numChunks = numChunks;
    spec.outputCodec = "libx264";

    TimeBasedSplitStrategy splitter;
    std::vector<ChunkSpec> chunks = splitter.split(spec);

    FFmpegExecutor exec;
    std::vector<std::string> produced;
    for (auto &c : chunks)
    {
        TaskResult r = exec.run(c, spec);
        if (r.status != TaskStatus::Success)
        {
            std::cerr << "Chunk " << c.chunkId << " failed: " << r.message << "\n";
            return 2;
        }
        produced.push_back(r.outputPath);
    }

    std::filesystem::path workdir = std::filesystem::path("work");
    std::filesystem::path listFile = workdir / "concat_list.txt";
    std::ofstream list(listFile);
    for (auto &p : produced)
    {
        list << "file '" << p << "'\n";
    }
    list.close();

    std::ostringstream cmd;
    cmd << "ffmpeg -y -f concat -safe 0 -i \"" << listFile.string() << "\" -c copy \"" << outFile << "\"";
    int rc = std::system(cmd.str().c_str());
    if (rc != 0)
    {
        std::cerr << "Failed to reassemble output (ffmpeg exit " << rc << ")\n";
        return 3;
    }

    std::cout << "Output written to " << outFile << "\n";
    return 0;
}
