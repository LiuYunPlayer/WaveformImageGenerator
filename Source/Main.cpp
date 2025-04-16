#include <JuceHeader.h>
#include <iostream>

using namespace juce;

constexpr int maxImageDimension = 16384;

// Print usage help
void printHelp()
{
    std::cout << R"(Usage: WaveformImageGenerator [options]

Options:
  -i <input file>      Input audio file path
  -o <output file>     Output PNG image path
  -s <start time>      Start time in seconds (default: 0)
  -e <end time>        End time in seconds, 0 means until end, negative means seconds from end (default: 0)
  -w <width>           Image width in pixels (default: 1920, max: 16384)
  -h <height>          Image height in pixels (default: 300, max: 16384)
  -b <RRGGBBAA>        Background color in RRGGBBAA hex (default: 000000FF)
  -f <RRGGBBAA>        Waveform color in RRGGBBAA hex (default: FFFFFFFF)
  --help               Show this help

Example:
  WaveformImageGenerator -i "song.wav" -o "waveform.png" -s 5 -e 30 -w 1920 -h 300 -b 1e1e1eff -f 00ffffff
)" << std::endl;
}

Colour parseHexColor(const String& hex)
{
    if (hex.length() != 8)
        return Colours::black;
    return Colour::fromRGBA(
        hex.substring(0, 2).getHexValue32(),
        hex.substring(2, 4).getHexValue32(),
        hex.substring(4, 6).getHexValue32(),
        hex.substring(6, 8).getHexValue32());
}

int wmain(int argc, wchar_t* argv[])
{
    std::setlocale(LC_ALL, "en_US.UTF-8");

    if (argc < 2)
    {
        printHelp();
        return 1;
    }

    String inputFile, outputFile;
    double startTime = 0.0, endTime = 0.0;
    int width = 1920, height = 300;
    Colour bgColor = Colours::transparentBlack;
    Colour fgColor = Colours::white;

    for (int i = 1; i < argc; ++i)
    {
        String key = String(argv[i]).trim();

        if (key == "--help")
        {
            printHelp();
            return 0;
        }
        else if (key == "-i" && i + 1 < argc) inputFile = String(argv[++i]);
        else if (key == "-o" && i + 1 < argc) outputFile = String(argv[++i]);
        else if (key == "-s" && i + 1 < argc) startTime = String(argv[++i]).getDoubleValue();
        else if (key == "-e" && i + 1 < argc) endTime = String(argv[++i]).getDoubleValue();
        else if (key == "-w" && i + 1 < argc) width = String(argv[++i]).getIntValue();
        else if (key == "-h" && i + 1 < argc) height = String(argv[++i]).getIntValue();
        else if (key == "-b" && i + 1 < argc) bgColor = parseHexColor(String(argv[++i]));
        else if (key == "-f" && i + 1 < argc) fgColor = parseHexColor(String(argv[++i]));
        else
        {
            printHelp();
            return 1;
        }
    }

    if (inputFile.isEmpty() || outputFile.isEmpty())
    {
        printHelp();
        return 1;
    }

    if (width > maxImageDimension || height > maxImageDimension)
    {
        std::cerr << "Image size too large. Max: " << maxImageDimension << std::endl;
        return 1;
    }

    std::cout << "=== Parameters ===" << std::endl;
    std::cout << "Input: " << inputFile << std::endl;
    std::cout << "Output: " << outputFile << std::endl;
    std::cout << "Start: " << startTime << " sec" << std::endl;
    std::cout << "End: " << endTime << " sec" << std::endl;
    std::cout << "Width: " << width << std::endl;
    std::cout << "Height: " << height << std::endl;
    std::cout << "Background color: " << bgColor.toDisplayString(true) << std::endl;
    std::cout << "Waveform color: " << fgColor.toDisplayString(true) << std::endl;

    File input(inputFile);
    if (!input.existsAsFile())
    {
        std::cerr << "Input file does not exist: " << input.getFullPathName().toRawUTF8() << std::endl;
        return 1;
    }

    AudioFormatManager formatManager;
    formatManager.registerBasicFormats();

    std::unique_ptr<AudioFormatReader> reader(formatManager.createReaderFor(input));
    if (!reader)
    {
        std::cerr << "Failed to read input audio file." << std::endl;
        return 1;
    }

    double duration = reader->lengthInSamples / reader->sampleRate;
    double actualEnd = endTime <= 0 ? (endTime < 0 ? duration + endTime : duration) : endTime;
    actualEnd = jmin(duration, actualEnd);
    double actualStart = jlimit(0.0, actualEnd, startTime);
    int64 startSample = static_cast<int64>(actualStart * reader->sampleRate);
    int64 numSamples = static_cast<int64>((actualEnd - actualStart) * reader->sampleRate);

    AudioBuffer<float> buffer(reader->numChannels, (int)numSamples);
    reader->read(&buffer, 0, (int)numSamples, startSample, true, true);

    Image image(Image::ARGB, width, height, true);
    Graphics g(image);
    g.fillAll(bgColor);

    int numChannels = buffer.getNumChannels();
    float channelHeight = (float)height / numChannels;

    g.setColour(fgColor);
    for (int ch = 0; ch < numChannels; ++ch) {
        auto* samples = buffer.getReadPointer(ch);

        float top = ch * channelHeight;
        float midY = top + channelHeight / 2.0f;

        for (int i = 0; i < width; ++i) {
            int sampleStart = (int)((i / (double)width) * numSamples);
            int sampleEnd = (int)(((i + 1) / (double)width) * numSamples);
            sampleStart = jlimit(0, (int)numSamples - 1, sampleStart);
            sampleEnd = jlimit(0, (int)numSamples, sampleEnd);

            float minVal = 1.0f, maxVal = -1.0f;
            for (int j = sampleStart; j < sampleEnd; ++j) {
                float s = samples[j];
                if (s < minVal) minVal = s;
                if (s > maxVal) maxVal = s;
            }

            float y1 = midY - minVal * (channelHeight / 2.0f);
            float y2 = midY - maxVal * (channelHeight / 2.0f);

            float rectY = jmin(y1, y2);
            float rectH = std::abs(y2 - y1);

            g.drawLine((float)i + 0.5f, y1, (float)i + 0.5f, y2);
        }
    }

    File output(outputFile);
    if (output.existsAsFile())
        output.deleteFile();

    PNGImageFormat pngFormat;
    FileOutputStream outputStream(output);

    if (!pngFormat.writeImageToStream(image, outputStream))
    {
        std::cerr << "Failed to save image." << std::endl;
        return 1;
    }

    outputStream.flush();
    std::cout << "Waveform image saved to: " << output.getFullPathName().toRawUTF8() << std::endl;
    return 0;
}