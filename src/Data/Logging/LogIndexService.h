#ifndef EOLO_LOG_INDEX_SERVICE_H
#define EOLO_LOG_INDEX_SERVICE_H

#include <Arduino.h>
#include <SD.h>
#include <RTClib.h>
#include <algorithm>
#include <vector>
#include "../../Config/Legacy.h"

// Canonical catalogue for the capture files stored under /EOLO/logs.  It is
// deliberately kept independent from Context: LogService calls it from the SD
// worker after the final row has reached the capture CSV.
class LogIndexService
{
public:
    static constexpr const char *CsvPath = "/EOLO/log_index.csv";
    static constexpr const char *HtmlPath = "/EOLO/log_index.html";
    static constexpr const char *CsvHeader =
        "log_file,start_date,start_time,end_date,end_time,captured_volume_l,volume_source";

    enum class VolumeSource : uint8_t { Recorded, EstimatedFlow, Unavailable };

    struct ReconcileSummary
    {
        uint32_t examined = 0;
        uint32_t current = 0;
        uint32_t recovered = 0;
        uint32_t incompatible = 0;
        uint32_t errors = 0;
        uint32_t validRows = 0;
        uint32_t ignoredRows = 0;
        bool rebuiltIndex = false;
    };

    struct Entry
    {
        String logFile;
        String startDate;
        String startTime;
        String endDate;
        String endTime;
        float capturedVolume = 0.0f;
        bool volumeAvailable = true;
        VolumeSource volumeSource = VolumeSource::Recorded;
    };

    bool reconcile(const char *logsDir, bool force = false);
    bool upsert(const Entry &entry);
    bool remove(const char *logFile);
    bool makeEntryFromCapture(const char *logFile, uint32_t startUnix,
                              uint32_t endUnix, float capturedVolume,
                              Entry &entry) const;
    ReconcileSummary reconciliationSummary() const;
    static const char *volumeSourceName(VolumeSource source);

private:
    static bool parseTimestamp(const String &text, uint32_t &unixTime);
    static bool timestampFromFilename(const String &name, uint32_t &unixTime);
    static void splitTimestamp(uint32_t unixTime, String &date, String &time);
    static int fieldIndex(const String &header, const char *field);
    static String fieldAt(const String &row, int index);
    static bool validBasename(const String &name);
    static bool lessEntry(const Entry &a, const Entry &b);
    static String htmlEscape(const String &value);
    enum class ReadResult : uint8_t { OkCurrent, OkLegacy, Empty, MissingTime, NoValidTimestamp, ReadError };
    bool readCapture(const char *path, const String &name, Entry &entry,
                     ReadResult &result, uint32_t &validRows, uint32_t &ignoredRows) const;
    bool loadCsv(std::vector<Entry> &entries) const;
    bool writeCsv(const std::vector<Entry> &entries) const;
    bool writeHtml(const std::vector<Entry> &entries) const;
    bool replaceFile(const char *temporary, const char *destination) const;
    bool writeAll(std::vector<Entry> &entries) const;
    mutable portMUX_TYPE _summaryMux = portMUX_INITIALIZER_UNLOCKED;
    ReconcileSummary _summary;
};

inline const char *LogIndexService::volumeSourceName(VolumeSource source)
{
    switch (source)
    {
    case VolumeSource::Recorded: return "recorded";
    case VolumeSource::EstimatedFlow: return "estimated_flow";
    case VolumeSource::Unavailable: return "unavailable";
    }
    return "unavailable";
}

inline LogIndexService::ReconcileSummary LogIndexService::reconciliationSummary() const
{
    portENTER_CRITICAL(&_summaryMux);
    ReconcileSummary copy = _summary;
    portEXIT_CRITICAL(&_summaryMux);
    return copy;
}

inline String LogIndexService::fieldAt(const String &row, int index)
{
    if (index < 0)
        return String();
    int current = 0;
    int start = 0;
    for (size_t i = 0; i <= row.length(); ++i)
    {
        if (i == row.length() || row.charAt(i) == ',')
        {
            if (current == index)
                return row.substring(start, i);
            start = static_cast<int>(i) + 1;
            ++current;
        }
    }
    return String();
}

inline int LogIndexService::fieldIndex(const String &header, const char *field)
{
    for (int i = 0;; ++i)
    {
        String value = fieldAt(header, i);
        if (value == field)
            return i;
        int commaCount = 0;
        for (size_t p = 0; p < header.length(); ++p)
            if (header.charAt(p) == ',')
                ++commaCount;
        if (i >= commaCount)
            return -1;
    }
}

inline bool LogIndexService::parseTimestamp(const String &value, uint32_t &unixTime)
{
    String text = value;
    text.trim();
    if (text.length() < 19)
        return false;
    const bool separators = text.charAt(4) == '-' && text.charAt(7) == '-' &&
                            (text.charAt(10) == 'T' || text.charAt(10) == ' ') &&
                            text.charAt(13) == ':' && text.charAt(16) == ':';
    if (!separators)
        return false;
    for (int i : {0, 1, 2, 3, 5, 6, 8, 9, 11, 12, 14, 15, 17, 18})
        if (!isDigit(text.charAt(i)))
            return false;

    const int year = text.substring(0, 4).toInt();
    const int month = text.substring(5, 7).toInt();
    const int day = text.substring(8, 10).toInt();
    const int hour = text.substring(11, 13).toInt();
    const int minute = text.substring(14, 16).toInt();
    const int second = text.substring(17, 19).toInt();
    if (year < 2000 || year > 2099 || month < 1 || month > 12 || day < 1 ||
        day > 31 || hour > 23 || minute > 59 || second > 59)
        return false;
    static const uint8_t daysPerMonth[] = {31, 28, 31, 30, 31, 30,
                                            31, 31, 30, 31, 30, 31};
    int maxDay = daysPerMonth[month - 1];
    const bool leapYear = (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
    if (month == 2 && leapYear)
        ++maxDay;
    if (day > maxDay)
        return false;
    DateTime parsed(year, month, day, hour, minute, second);
    if (parsed.year() != year || parsed.month() != month || parsed.day() != day ||
        parsed.hour() != hour || parsed.minute() != minute || parsed.second() != second)
        return false;
    unixTime = parsed.unixtime();
    return true;
}

inline bool LogIndexService::timestampFromFilename(const String &name, uint32_t &unixTime)
{
    if (!name.startsWith("log_") || name.length() < 27)
        return false;
    String stamp = name.substring(4, 23); // YYYY_MM_DDTHH_MM_SS
    if (stamp.charAt(4) != '_' || stamp.charAt(7) != '_' || stamp.charAt(10) != 'T' ||
        stamp.charAt(13) != '_' || stamp.charAt(16) != '_')
        return false;
    stamp.setCharAt(4, '-');
    stamp.setCharAt(7, '-');
    stamp.setCharAt(13, ':');
    stamp.setCharAt(16, ':');
    return parseTimestamp(stamp, unixTime);
}

inline void LogIndexService::splitTimestamp(uint32_t unixTime, String &date, String &time)
{
    String stamp = DateTime(unixTime).timestamp();
    date = stamp.substring(0, 10);
    time = stamp.substring(11, 19);
}

inline bool LogIndexService::validBasename(const String &name)
{
    if (!name.startsWith("log_") || !name.endsWith(".csv"))
        return false;
    for (size_t i = 0; i < name.length(); ++i)
    {
        const char c = name.charAt(i);
        if (!(isAlphaNumeric(c) || c == '_' || c == '-' || c == '.'))
            return false;
    }
    return true;
}

inline bool LogIndexService::lessEntry(const Entry &a, const Entry &b)
{
    String aStamp = a.startDate + "T" + a.startTime;
    String bStamp = b.startDate + "T" + b.startTime;
    if (aStamp != bStamp)
        return aStamp < bStamp;
    return a.logFile < b.logFile;
}

inline bool LogIndexService::makeEntryFromCapture(const char *logFile,
                                                   uint32_t startUnix,
                                                   uint32_t endUnix,
                                                   float capturedVolume,
                                                   Entry &entry) const
{
    if (logFile == nullptr || !validBasename(String(logFile)) || startUnix == 0 || endUnix == 0)
        return false;
    entry.logFile = logFile;
    splitTimestamp(startUnix, entry.startDate, entry.startTime);
    splitTimestamp(endUnix, entry.endDate, entry.endTime);
    entry.capturedVolume = isfinite(capturedVolume) ? capturedVolume : 0.0f;
    entry.volumeAvailable = isfinite(capturedVolume);
    entry.volumeSource = entry.volumeAvailable ? VolumeSource::Recorded : VolumeSource::Unavailable;
    return true;
}

inline bool LogIndexService::readCapture(const char *path, const String &name, Entry &entry,
                                         ReadResult &result, uint32_t &validRows,
                                         uint32_t &ignoredRows) const
{
    validRows = ignoredRows = 0;
    File file = SD.open(path, FILE_READ);
    if (!file) {
        result = ReadResult::ReadError;
        return false;
    }
    String header = file.readStringUntil('\n');
    header.trim();
    if (header.length() == 0) {
        file.close();
        result = ReadResult::Empty;
        return false;
    }
    const int timeColumn = fieldIndex(header, "time");
    const int volumeColumn = fieldIndex(header, "captured_volume");
    const int flowColumn = fieldIndex(header, "flow");
    if (timeColumn < 0)
    {
        file.close();
        result = ReadResult::MissingTime;
        return false;
    }

    uint32_t firstUnix = 0;
    uint32_t lastUnix = 0;
    float lastVolume = 0.0f;
    float estimatedVolume = 0.0f;
    bool foundVolume = false;
    bool foundFlow = false;
    bool found = false;
    bool sawDataRow = false;
    while (file.available())
    {
        String row = file.readStringUntil('\n');
        row.trim();
        if (row.length() == 0)
            continue;
        sawDataRow = true;
        uint32_t rowUnix = 0;
        if (!parseTimestamp(fieldAt(row, timeColumn), rowUnix) ||
            (found && rowUnix < lastUnix))
        {
            ++ignoredRows;
            continue;
        }
        if (!found)
            firstUnix = rowUnix;
        lastUnix = rowUnix;
        found = true;
        ++validRows;

        if (volumeColumn >= 0) {
            String text = fieldAt(row, volumeColumn);
            char *end = nullptr;
            float value = strtof(text.c_str(), &end);
            if (end != text.c_str() && *end == '\0' && isfinite(value)) {
                lastVolume = value;
                foundVolume = true;
            }
        } else if (flowColumn >= 0) {
            String text = fieldAt(row, flowColumn);
            char *end = nullptr;
            float flow = strtof(text.c_str(), &end);
            if (end != text.c_str() && *end == '\0' && isfinite(flow) && flow >= 0.0f) {
                estimatedVolume += flow * (10.0f / 60.0f);
                foundFlow = true;
            }
        }
    }
    file.close();
    if (!found) {
        result = sawDataRow ? ReadResult::NoValidTimestamp : ReadResult::Empty;
        return false;
    }

    uint32_t startUnix = 0;
    if (!timestampFromFilename(name, startUnix))
        startUnix = firstUnix;
    const float volume = foundVolume ? lastVolume : (foundFlow ? estimatedVolume : NAN);
    if (!makeEntryFromCapture(name.c_str(), startUnix, lastUnix, volume, entry)) {
        result = ReadResult::ReadError;
        return false;
    }
    entry.volumeAvailable = foundVolume || foundFlow;
    entry.volumeSource = foundVolume ? VolumeSource::Recorded :
                         (foundFlow ? VolumeSource::EstimatedFlow : VolumeSource::Unavailable);
    result = volumeColumn >= 0 ? ReadResult::OkCurrent : ReadResult::OkLegacy;
    return true;
}

inline bool LogIndexService::loadCsv(std::vector<Entry> &entries) const
{
    entries.clear();
    File file = SD.open(CsvPath, FILE_READ);
    if (!file)
        return false;
    String header = file.readStringUntil('\n');
    header.trim();
    if (header != CsvHeader)
    {
        file.close();
        return false;
    }
    while (file.available())
    {
        String row = file.readStringUntil('\n');
        row.trim();
        if (row.length() == 0)
            continue;
        int commaCount = 0;
        for (size_t i = 0; i < row.length(); ++i)
        {
            if (row.charAt(i) == ',')
                ++commaCount;
        }
        if (commaCount != 6)
        {
            file.close();
            entries.clear();
            return false;
        }
        Entry entry;
        entry.logFile = fieldAt(row, 0);
        entry.startDate = fieldAt(row, 1);
        entry.startTime = fieldAt(row, 2);
        entry.endDate = fieldAt(row, 3);
        entry.endTime = fieldAt(row, 4);
        String volumeText = fieldAt(row, 5);
        String sourceText = fieldAt(row, 6);
        entry.volumeAvailable = volumeText.length() != 0;
        char *end = nullptr;
        entry.capturedVolume = entry.volumeAvailable ? strtof(volumeText.c_str(), &end) : 0.0f;
        if (sourceText == "recorded") entry.volumeSource = VolumeSource::Recorded;
        else if (sourceText == "estimated_flow") entry.volumeSource = VolumeSource::EstimatedFlow;
        else if (sourceText == "unavailable") entry.volumeSource = VolumeSource::Unavailable;
        else { file.close(); entries.clear(); return false; }
        uint32_t ignoredStart = 0, ignoredEnd = 0;
        if (!validBasename(entry.logFile) || !parseTimestamp(entry.startDate + "T" + entry.startTime, ignoredStart) ||
            !parseTimestamp(entry.endDate + "T" + entry.endTime, ignoredEnd) ||
            (entry.volumeAvailable && (end == volumeText.c_str() || *end != '\0' || !isfinite(entry.capturedVolume))) ||
            (!entry.volumeAvailable && entry.volumeSource != VolumeSource::Unavailable))
        {
            file.close();
            entries.clear();
            return false;
        }
        for (const Entry &existing : entries)
            if (existing.logFile == entry.logFile)
            {
                file.close();
                entries.clear();
                return false;
            }
        entries.push_back(entry);
        if (entries.size() > 1 && lessEntry(entries.back(), entries[entries.size() - 2]))
        {
            file.close();
            entries.clear();
            return false;
        }
    }
    file.close();
    return true;
}

inline bool LogIndexService::replaceFile(const char *temporary, const char *destination) const
{
    String backup = String(destination) + ".bak";
    if (SD.exists(backup.c_str()))
        SD.remove(backup.c_str());
    const bool hadDestination = SD.exists(destination);
    if (hadDestination && !SD.rename(destination, backup.c_str()))
        return false;
    if (!SD.rename(temporary, destination))
    {
        if (hadDestination)
            SD.rename(backup.c_str(), destination);
        return false;
    }
    if (hadDestination)
        SD.remove(backup.c_str());
    return true;
}

inline bool LogIndexService::writeCsv(const std::vector<Entry> &entries) const
{
    const char *temporary = "/EOLO/log_index.csv.tmp";
    if (SD.exists(temporary))
        SD.remove(temporary);
    File file = SD.open(temporary, FILE_WRITE);
    if (!file)
        return false;
    file.println(CsvHeader);
    for (const Entry &entry : entries)
    {
        file.print(entry.logFile); file.print(',');
        file.print(entry.startDate); file.print(',');
        file.print(entry.startTime); file.print(',');
        file.print(entry.endDate); file.print(',');
        file.print(entry.endTime); file.print(',');
        if (entry.volumeAvailable) file.print(entry.capturedVolume, 3);
        file.print(',');
        file.println(volumeSourceName(entry.volumeSource));
    }
    file.close();
    return replaceFile(temporary, CsvPath);
}

inline String LogIndexService::htmlEscape(const String &value)
{
    String out;
    out.reserve(value.length() + 8);
    for (size_t i = 0; i < value.length(); ++i)
    {
        switch (value.charAt(i))
        {
        case '&': out += F("&amp;"); break;
        case '<': out += F("&lt;"); break;
        case '>': out += F("&gt;"); break;
        case '"': out += F("&quot;"); break;
        default: out += value.charAt(i); break;
        }
    }
    return out;
}

inline bool LogIndexService::writeHtml(const std::vector<Entry> &entries) const
{
    const char *temporary = "/EOLO/log_index.html.tmp";
    if (SD.exists(temporary))
        SD.remove(temporary);
    File file = SD.open(temporary, FILE_WRITE);
    if (!file)
        return false;
    file.println(F("<!doctype html><html lang=\"es\"><meta charset=\"utf-8\"><meta name=\"viewport\" content=\"width=device-width\"><title>Capturas EOLO</title>"));
    file.println(F("<style>body{font-family:sans-serif;margin:2rem}table{border-collapse:collapse}th,td{border:1px solid #bbb;padding:.4rem;text-align:left}tbody tr:nth-child(even){background:#f4f4f4}</style><h1>Capturas EOLO</h1><table><thead><tr><th>Archivo</th><th>Fecha inicio</th><th>Hora inicio</th><th>Fecha fin</th><th>Hora fin</th><th>Volumen capturado (L)</th><th>Origen</th></tr></thead><tbody>"));
    for (const Entry &entry : entries)
    {
        const String escaped = htmlEscape(entry.logFile);
        file.print(F("<tr><td><a href=\"logs/")); file.print(escaped); file.print(F("\">"));
        file.print(escaped); file.print(F("</a></td><td>")); file.print(entry.startDate);
        file.print(F("</td><td>")); file.print(entry.startTime); file.print(F("</td><td>"));
        file.print(entry.endDate); file.print(F("</td><td>")); file.print(entry.endTime);
        file.print(F("</td><td>"));
        if (entry.volumeAvailable) file.print(entry.capturedVolume, 3);
        file.print(F("</td><td>")); file.print(volumeSourceName(entry.volumeSource)); file.println(F("</td></tr>"));
    }
    file.println(F("</tbody></table></html>"));
    file.close();
    return replaceFile(temporary, HtmlPath);
}

inline bool LogIndexService::writeAll(std::vector<Entry> &entries) const
{
    std::sort(entries.begin(), entries.end(), lessEntry);
    return writeCsv(entries) && writeHtml(entries);
}

inline bool LogIndexService::upsert(const Entry &entry)
{
    std::vector<Entry> entries;
    if (SD.exists(CsvPath) && !loadCsv(entries))
        return false;
    bool replaced = false;
    for (Entry &existing : entries)
        if (existing.logFile == entry.logFile)
        {
            existing = entry;
            replaced = true;
            break;
        }
    if (!replaced)
        entries.push_back(entry);
    const bool ok = writeAll(entries);
    if (ok)
    {
        portENTER_CRITICAL(&_summaryMux);
        _summary.examined = entries.size();
        _summary.current = 0;
        _summary.recovered = 0;
        for (const Entry &e : entries)
        {
            if (e.volumeSource == VolumeSource::Recorded)
                ++_summary.current;
            else
                ++_summary.recovered;
        }
        portEXIT_CRITICAL(&_summaryMux);
    }
    return ok;
}

inline bool LogIndexService::remove(const char *logFile)
{
    std::vector<Entry> entries;
    if (!loadCsv(entries))
        return false;
    entries.erase(std::remove_if(entries.begin(), entries.end(),
                                 [logFile](const Entry &entry) { return entry.logFile == logFile; }),
                  entries.end());
    const bool ok = writeAll(entries);
    if (ok)
    {
        portENTER_CRITICAL(&_summaryMux);
        _summary.examined = entries.size();
        _summary.current = 0;
        _summary.recovered = 0;
        for (const Entry &e : entries)
        {
            if (e.volumeSource == VolumeSource::Recorded)
                ++_summary.current;
            else
                ++_summary.recovered;
        }
        portEXIT_CRITICAL(&_summaryMux);
    }
    return ok;
}

inline bool LogIndexService::reconcile(const char *logsDir, bool force)
{
    std::vector<Entry> entries;
    const bool masterValid = !force && loadCsv(entries);
    if (masterValid)
    {
        ReconcileSummary summary;
        summary.examined = entries.size();
        for (const Entry &e : entries)
        {
            if (e.volumeSource == VolumeSource::Recorded)
                ++summary.current;
            else
                ++summary.recovered;
        }
        summary.rebuiltIndex = false;
        portENTER_CRITICAL(&_summaryMux);
        _summary = summary;
        portEXIT_CRITICAL(&_summaryMux);
        LOG_F("Indice maestro valido (%u capturas); reconciliacion omitida.\n",
              (unsigned int)entries.size());

        if (!SD.exists(HtmlPath))
        {
            writeHtml(entries);
        }
        return true;
    }

    ReconcileSummary summary;
    summary.rebuiltIndex = true;
    LOG_LN("Indice maestro ausente o corrupto; ejecutando reconciliacion.");

    File directory = SD.open(logsDir);
    if (!directory || !directory.isDirectory())
        return false;
    entries.clear();
    File file = directory.openNextFile();
    while (file)
    {
        String fullName = file.name();
        const bool isDirectory = file.isDirectory();
        file.close();
        int slash = fullName.lastIndexOf('/');
        String name = slash >= 0 ? fullName.substring(slash + 1) : fullName;
        if (!isDirectory && validBasename(name))
        {
            ++summary.examined;
            Entry recovered;
            String path = String(logsDir) + "/" + name;
            ReadResult result = ReadResult::ReadError;
            uint32_t validRows = 0, ignoredRows = 0;
            if (readCapture(path.c_str(), name, recovered, result, validRows, ignoredRows)) {
                entries.push_back(recovered);
                if (result == ReadResult::OkCurrent) ++summary.current;
                else ++summary.recovered;
                summary.validRows += validRows;
                summary.ignoredRows += ignoredRows;
                LOG_F("Indice: %s esquema=%s filas_validas=%lu ignoradas=%lu volumen=%s\n",
                      name.c_str(), result == ReadResult::OkCurrent ? "actual" : "historico",
                      (unsigned long)validRows, (unsigned long)ignoredRows,
                      volumeSourceName(recovered.volumeSource));
            } else {
                const char *reason = "error_lectura";
                if (result == ReadResult::Empty) reason = "archivo_vacio";
                else if (result == ReadResult::MissingTime) reason = "columna_time_faltante";
                else if (result == ReadResult::NoValidTimestamp) reason = "timestamp_invalido";
                if (result == ReadResult::ReadError) ++summary.errors;
                else ++summary.incompatible;
                LOG_F("Indice: %s omitido motivo=%s filas_ignoradas=%lu\n",
                      name.c_str(), reason, (unsigned long)ignoredRows);
            }
        }
        file = directory.openNextFile();
    }
    directory.close();
    // Always rewrite both files: the HTML is a disposable view and this also
    // repairs stale/missing HTML after interrupted writes.
    const bool written = writeAll(entries);
    if (!written) ++summary.errors;
    portENTER_CRITICAL(&_summaryMux);
    _summary = summary;
    portEXIT_CRITICAL(&_summaryMux);
    LOG_F("Indice reconciliado: examinados=%lu actuales=%lu recuperados=%lu incompatibles=%lu errores=%lu\n",
          (unsigned long)summary.examined, (unsigned long)summary.current,
          (unsigned long)summary.recovered, (unsigned long)summary.incompatible,
          (unsigned long)summary.errors);
    return written;
}

#endif // EOLO_LOG_INDEX_SERVICE_H
