#include "TitleParser.h"
#include <QFileInfo>

#include <QRegularExpression>
#include <QSet>
#include <QStringList>

namespace {

QRegularExpression rx(const QString &pattern, QRegularExpression::PatternOptions options = QRegularExpression::CaseInsensitiveOption)
{
    return QRegularExpression(pattern, options);
}

QString firstMatch(const QVector<QPair<QRegularExpression, QString>> &patterns, const QString &text)
{
    for (const auto &[pattern, value] : patterns) {
        if (pattern.match(text).hasMatch())
            return value;
    }
    return {};
}

int firstInt(const QVector<QPair<QRegularExpression, int>> &patterns, const QString &text)
{
    for (const auto &[pattern, value] : patterns) {
        if (pattern.match(text).hasMatch())
            return value;
    }
    return 0;
}

QString subtitleLangs(const QString &title)
{
    static const QRegularExpression hans(QStringLiteral("[简簡]|CHS|JPSC|(?<![A-Za-z])SC(?![A-Za-z])|zh-?Hans|GB[&_+]?BIG5"),
                                         QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression hant(QStringLiteral("繁|正體|CHT|JPTC|BIG5|(?<![A-Za-z])TC(?![A-Za-z])|zh-?Hant"));
    static const QRegularExpression zh(QStringLiteral("中[文字]|(?<![A-Za-z])CHI(?![A-Za-z])"));
    static const QRegularExpression ja(QStringLiteral("[简繁簡]日|日[简繁簡双雙内內]|JP(?:SC|TC)|日[文语語]字幕"));
    static const QRegularExpression en(QStringLiteral("(?<![A-Za-z])(?:ENG|English)(?![A-Za-z])|英[文语語]|[简繁簡]英|英[简繁簡]"));
    static const QRegularExpression multi(QStringLiteral("Multi[\\s\\-_]?Subs?|Multiple Subtitle|多[语語]字幕"),
                                          QRegularExpression::CaseInsensitiveOption);

    QStringList langs;
    if (hans.match(title).hasMatch())
        langs << QStringLiteral("zh-Hans");
    if (hant.match(title).hasMatch())
        langs << QStringLiteral("zh-Hant");
    if (langs.isEmpty() && zh.match(title).hasMatch())
        langs << QStringLiteral("zh");
    if (ja.match(title).hasMatch())
        langs << QStringLiteral("ja");
    if (en.match(title).hasMatch())
        langs << QStringLiteral("en");
    if (multi.match(title).hasMatch())
        langs << QStringLiteral("multi");
    return langs.join(QLatin1Char(','));
}

QString subtitleType(const QString &title)
{
    static const QVector<QPair<QRegularExpression, QString>> patterns = {
        {rx(QStringLiteral("[内內]封|[内內]挂|[内內]掛|soft-?subs?")), QStringLiteral("internal")},
        {rx(QStringLiteral("[内內]嵌|硬字幕|hard-?subs?")), QStringLiteral("hardcoded")},
        {rx(QStringLiteral("外挂|外掛"), QRegularExpression::NoPatternOption), QStringLiteral("external")},
        {rx(QStringLiteral("(?<![A-Za-z])PGS(?![A-Za-z])")), QStringLiteral("pgs")},
        {rx(QStringLiteral("(?<![A-Za-z])ASS(?![A-Za-z])"), QRegularExpression::NoPatternOption), QStringLiteral("ass")},
        {rx(QStringLiteral("(?<![A-Za-z])SRT(?![A-Za-z])")), QStringLiteral("srt")},
    };
    return firstMatch(patterns, title);
}

QString sourceType(const QString &title)
{
    static const QVector<QPair<QRegularExpression, QString>> patterns = {
        {rx(QStringLiteral("(?<![A-Za-z0-9])(?:BDMV|Remux|BD-?BOX)(?![A-Za-z0-9])")), QStringLiteral("Blu-ray")},
        {rx(QStringLiteral("(?<![A-Za-z0-9])(?:BD-?Rip|Blu-?ray|BD)(?![A-Za-z0-9])")), QStringLiteral("BDRip")},
        {rx(QStringLiteral("(?<![A-Za-z0-9])WEB-?DL(?![A-Za-z0-9])")), QStringLiteral("WEB-DL")},
        {rx(QStringLiteral("(?<![A-Za-z0-9])WEB-?Rip(?![A-Za-z0-9])")), QStringLiteral("WEBRip")},
        {rx(QStringLiteral("(?<![A-Za-z0-9])(?:WEB|Baha|ABEMA|AMZN|Netflix|B-Global|Bilibili)(?![A-Za-z0-9])")), QStringLiteral("WEB")},
        {rx(QStringLiteral("(?<![A-Za-z])(?:CR|NF)(?![A-Za-z])")), QStringLiteral("WEB")},
        {rx(QStringLiteral("(?<![A-Za-z0-9])(?:DVD-?Rip|DVD)(?![A-Za-z0-9])")), QStringLiteral("DVDRip")},
        {rx(QStringLiteral("(?<![A-Za-z0-9])(?:HDTV|TV-?Rip)(?![A-Za-z0-9])")), QStringLiteral("TV")},
    };
    return firstMatch(patterns, title);
}

int chineseInt(const QString &text)
{
    static const QHash<QChar, int> digits = {
        {QChar(u'一'), 1}, {QChar(u'二'), 2}, {QChar(u'三'), 3}, {QChar(u'四'), 4},
        {QChar(u'五'), 5}, {QChar(u'六'), 6}, {QChar(u'七'), 7}, {QChar(u'八'), 8},
        {QChar(u'九'), 9},
    };
    bool ok = false;
    const int direct = text.toInt(&ok);
    if (ok)
        return direct;
    if (text == QStringLiteral("十"))
        return 10;
    if (text.startsWith(QStringLiteral("十")))
        return 10 + digits.value(text.at(1), 0);
    if (text.endsWith(QStringLiteral("十")))
        return digits.value(text.at(0), 0) * 10;
    const int idx = text.indexOf(QStringLiteral("十"));
    if (idx > 0) {
        const int tens = digits.value(text.at(idx - 1), 0) * 10;
        const int ones = idx + 1 < text.size() ? digits.value(text.at(idx + 1), 0) : 0;
        return tens + ones;
    }
    return digits.value(text.isEmpty() ? QChar() : text.at(0), 0);
}

} // namespace

bool isCjk(const QString &text)
{
    for (const QChar c : text) {
        const ushort u = c.unicode();
        if ((u >= 0x3040 && u <= 0x30ff) || (u >= 0x3400 && u <= 0x9fff))
            return true;
    }
    return false;
}

ResourceTags parseTitle(const QString &title)
{
    ResourceTags tags;
    const auto guessed = Confidence::Guessed;
    const auto unknown = Confidence::Unknown;

    static const QVector<QPair<QRegularExpression, QString>> resolutions = {
        {rx(QStringLiteral("(?<![A-Za-z0-9])(?:4320p|8K)(?![A-Za-z0-9])|7680\\s*[x×]\\s*4320")), QStringLiteral("4320p")},
        {rx(QStringLiteral("(?<![A-Za-z0-9])(?:2160p|4K|UHD)(?![A-Za-z0-9])|(?:3840|4096)\\s*[x×]\\s*2160")), QStringLiteral("2160p")},
        {rx(QStringLiteral("(?<![A-Za-z0-9])1440p(?![A-Za-z0-9])|2560\\s*[x×]\\s*1440")), QStringLiteral("1440p")},
        {rx(QStringLiteral("(?<![A-Za-z0-9])1080[pi](?![A-Za-z0-9])|1920\\s*[x×]\\s*10[78]\\d")), QStringLiteral("1080p")},
        {rx(QStringLiteral("(?<![A-Za-z0-9])720p(?![A-Za-z0-9])|1280\\s*[x×]\\s*720")), QStringLiteral("720p")},
        {rx(QStringLiteral("(?<![A-Za-z0-9])(?:480|576)p(?![A-Za-z0-9])|(?:640|720|848|854)\\s*[x×]\\s*480")), QStringLiteral("480p")},
    };
    tags.resolution = firstMatch(resolutions, title);
    tags.resolutionConf = tags.resolution.isEmpty() ? unknown : guessed;

    static const QVector<QPair<QRegularExpression, QString>> codecs = {
        {rx(QStringLiteral("(?<![A-Za-z0-9])AV1(?![A-Za-z0-9])")), QStringLiteral("AV1")},
        {rx(QStringLiteral("(?<![A-Za-z0-9])VP9(?![A-Za-z0-9])")), QStringLiteral("VP9")},
        {rx(QStringLiteral("(?<![A-Za-z0-9])(?:HEVC|[HX]\\.?265)(?![A-Za-z0-9])")), QStringLiteral("HEVC")},
        {rx(QStringLiteral("(?<![A-Za-z0-9])Hi10P?(?![A-Za-z0-9])")), QStringLiteral("Hi10P")},
        {rx(QStringLiteral("(?<![A-Za-z0-9])(?:AVC|[HX]\\.?264)(?![A-Za-z0-9])")), QStringLiteral("H.264")},
        {rx(QStringLiteral("(?<![A-Za-z0-9])MPEG-?2(?![A-Za-z0-9])")), QStringLiteral("MPEG-2")},
    };
    tags.videoCodec = firstMatch(codecs, title);

    static const QVector<QPair<QRegularExpression, int>> bitDepths = {
        {rx(QStringLiteral("(?<![A-Za-z0-9])12[\\s\\-_]?bits?(?![A-Za-z0-9])")), 12},
        {rx(QStringLiteral("(?<![A-Za-z0-9])(?:10[\\s\\-_]?bits?|Main\\s?10|Ma10p|Hi10P?)(?![A-Za-z0-9])")), 10},
        {rx(QStringLiteral("(?<![A-Za-z0-9])8[\\s\\-_]?bits?(?![A-Za-z0-9])")), 8},
    };
    tags.bitDepth = firstInt(bitDepths, title);
    tags.videoConf = (tags.videoCodec.isEmpty() && tags.bitDepth == 0) ? unknown : guessed;

    static const QVector<QPair<QRegularExpression, QString>> hdrTypes = {
        {rx(QStringLiteral("(?<![A-Za-z0-9])(?:Dolby[\\s.]?Vision|DoVi|DV)(?![A-Za-z0-9])")), QStringLiteral("DolbyVision")},
        {rx(QStringLiteral("(?<![A-Za-z0-9])HDR10(?:\\+|Plus)")), QStringLiteral("HDR10+")},
        {rx(QStringLiteral("(?<![A-Za-z0-9])HDR10(?![A-Za-z0-9])")), QStringLiteral("HDR10")},
        {rx(QStringLiteral("(?<![A-Za-z0-9])HLG(?![A-Za-z0-9])")), QStringLiteral("HLG")},
        {rx(QStringLiteral("(?<![A-Za-z0-9])HDR(?![A-Za-z0-9])")), QStringLiteral("HDR")},
        {rx(QStringLiteral("(?<![A-Za-z0-9])SDR(?![A-Za-z0-9])")), QStringLiteral("SDR")},
    };
    tags.hdrType = firstMatch(hdrTypes, title);
    if (tags.hdrType == QStringLiteral("DolbyVision")) {
        static const QVector<QPair<QRegularExpression, QString>> fallback = {
            {rx(QStringLiteral("(?<![A-Za-z0-9])HDR10(?:\\+|Plus)")), QStringLiteral("HDR10+")},
            {rx(QStringLiteral("(?<![A-Za-z0-9])HDR10(?![A-Za-z0-9])")), QStringLiteral("HDR10")},
            {rx(QStringLiteral("(?<![A-Za-z0-9])HLG(?![A-Za-z0-9])")), QStringLiteral("HLG")},
        };
        tags.hdrFallback = firstMatch(fallback, title);
    }
    tags.hdrConf = tags.hdrType.isEmpty() ? unknown : guessed;

    static const QVector<QPair<QRegularExpression, QString>> audio = {
        {rx(QStringLiteral("(?<![A-Za-z0-9])(?:Dolby[\\s.]?)?Atmos(?![A-Za-z0-9])")), QStringLiteral("Dolby Atmos")},
        {rx(QStringLiteral("(?<![A-Za-z0-9])TrueHD(?![A-Za-z])")), QStringLiteral("TrueHD")},
        {rx(QStringLiteral("(?<![A-Za-z0-9])DTS[\\s\\-:]?X(?![A-Za-z0-9])")), QStringLiteral("DTS:X")},
        {rx(QStringLiteral("(?<![A-Za-z0-9])DTS[\\s\\-]?HD[\\s\\-]?MA(?![A-Za-z])")), QStringLiteral("DTS-HD MA")},
        {rx(QStringLiteral("(?<![A-Za-z0-9])DTS[\\s\\-]?HD(?![A-Za-z])")), QStringLiteral("DTS-HD")},
        {rx(QStringLiteral("(?<![A-Za-z0-9])DTS(?![A-Za-z])")), QStringLiteral("DTS")},
        {rx(QStringLiteral("(?<![A-Za-z0-9])(?:E-?AC-?3|DDP|DD\\+)(?![A-Za-z])")), QStringLiteral("E-AC-3")},
        {rx(QStringLiteral("(?<![A-Za-z0-9])(?:AC-?3|DD)(?![A-Za-z])")), QStringLiteral("AC-3")},
        {rx(QStringLiteral("(?<![A-Za-z0-9])FLAC(?:x\\d)?(?![A-Za-z])")), QStringLiteral("FLAC")},
        {rx(QStringLiteral("(?<![A-Za-z0-9])L?PCM(?![A-Za-z])")), QStringLiteral("LPCM")},
        {rx(QStringLiteral("(?<![A-Za-z0-9])OPUS(?:x\\d)?(?![A-Za-z])")), QStringLiteral("Opus")},
        {rx(QStringLiteral("(?<![A-Za-z0-9])AAC(?:x\\d)?(?![A-Za-z])")), QStringLiteral("AAC")},
        {rx(QStringLiteral("(?<![A-Za-z0-9])MP3(?![A-Za-z0-9])")), QStringLiteral("MP3")},
    };
    tags.audioCodec = firstMatch(audio, title);
    static const QRegularExpression channels(QStringLiteral("(?<![vV\\d.])([1-7]\\.[01])(?![\\d.])"));
    const auto channelMatch = channels.match(title);
    if (channelMatch.hasMatch())
        tags.audioChannels = channelMatch.captured(1);
    tags.audioConf = tags.audioCodec.isEmpty() ? unknown : guessed;

    tags.subtitleLang = subtitleLangs(title);
    tags.subtitleType = subtitleType(title);
    tags.subtitleConf = (tags.subtitleLang.isEmpty() && tags.subtitleType.isEmpty()) ? unknown : guessed;
    tags.sourceType = sourceType(title);

    static const QRegularExpression groupRx(QStringLiteral("^\\s*[\\[【]([^\\]】]+)[\\]】]"));
    static const QRegularExpression tagLike(QStringLiteral("^\\s*(?:\\d{3,4}p|[\\d\\s.\\-]+|BD|WEB|MKV|MP4)\\s*$"),
                                            QRegularExpression::CaseInsensitiveOption);
    const auto groupMatch = groupRx.match(title);
    if (groupMatch.hasMatch() && !tagLike.match(groupMatch.captured(1)).hasMatch())
        tags.releaseGroup = groupMatch.captured(1).trimmed();

    const bool found = !tags.resolution.isEmpty() || !tags.videoCodec.isEmpty() || !tags.hdrType.isEmpty()
        || !tags.audioCodec.isEmpty() || !tags.subtitleLang.isEmpty();
    tags.confidence = found ? guessed : unknown;
    return tags;
}

EpisodeInfo parseEpisode(const QString &title)
{
    QString text=title.normalized(QString::NormalizationForm_KC);
    // A bracketed hexadecimal identifier (for example [E528]) is not an
    // episode. Mask it before trying any episode convention.
    static const QRegularExpression checksum(QStringLiteral("[\\[【]([A-Fa-f0-9]{4,64})[\\]】]"));
    auto codes=checksum.globalMatch(text);
    QVector<QPair<int,int>> masked;
    while(codes.hasNext()) {
        const auto code=codes.next();
        if(QRegularExpression("[A-Fa-f]").match(code.captured(1)).hasMatch())
            masked.append({code.capturedStart(),code.capturedLength()});
    }
    for(const auto &[start,length]:masked)for(int i=start;i<start+length;++i)text[i]=QChar(' ');
    // Long-running series (One Piece, Conan, ...) use four-digit numbers and
    // collections such as 1001-1050 or EP1001-EP1010. Years (2019-2020) and
    // resolutions are never episode numbers.
    static const QVector<QRegularExpression> ranges = {
        QRegularExpression(QStringLiteral("第\\s*(\\d{1,4})\\s*[-~～至]\\s*(\\d{1,4})\\s*[话話集]")),
        QRegularExpression(QStringLiteral("(?<![A-Za-z0-9])EP?\\.?\\s*(\\d{1,4})\\s*[-~～]\\s*(?:EP?\\.?\\s*)?(\\d{1,4})(?![\\dA-Za-z])"), QRegularExpression::CaseInsensitiveOption),
        QRegularExpression(QStringLiteral("(?<![\\dA-Za-z.])(\\d{1,4})\\s*[-~～]\\s*(\\d{1,4})(?![\\dA-Za-z.])")),
    };
    static const QSet<int> notEpisodes = {480, 576, 720, 1080, 1440, 2160, 4320};
    for (const auto &pattern : ranges) {
        auto it = pattern.globalMatch(text);
        while (it.hasNext()) {
            const auto match = it.next();
            const int a = match.captured(1).toInt();
            const int b = match.captured(2).toInt();
            if (a < b && b < 1900 && b - a < 1500 && !notEpisodes.contains(b))
                return {match.captured(1) + QLatin1Char('-') + match.captured(2), true};
        }
    }

    static const QRegularExpression batchRx(QStringLiteral("合集|全集|(?<![A-Za-z])(?:Batch|Complete|BD-?BOX)(?![A-Za-z])"),
                                             QRegularExpression::CaseInsensitiveOption);
    const bool batch = batchRx.match(text).hasMatch();

    if (!batch) {
        static const QVector<QRegularExpression> singles = {
            QRegularExpression(QStringLiteral("第\\s*(\\d{1,4}(?:\\.5)?)\\s*[话話集]")),
            QRegularExpression(QStringLiteral("(?<![A-Za-z0-9])S\\d{1,2}E(\\d{1,4})(?![\\dA-Za-z])"), QRegularExpression::CaseInsensitiveOption),
            QRegularExpression(QStringLiteral("(?<![A-Za-z0-9])EP\\.?\\s*(\\d{1,4})(?![\\dA-Za-z])"), QRegularExpression::CaseInsensitiveOption),
            QRegularExpression(QStringLiteral("[\\[【]\\s*(\\d{1,4}(?:\\.5)?)\\s*(?:v\\d)?\\s*(?:END|完|Fin)?\\s*[\\]】]"), QRegularExpression::CaseInsensitiveOption),
            QRegularExpression(QStringLiteral("\\s[-–]\\s(\\d{1,4}(?:\\.5)?)(?:v\\d)?(?:\\s*END)?(?=[\\s\\[(（.]|$)"), QRegularExpression::CaseInsensitiveOption),
            // Bare E followed by 3+ digits is ambiguous with release IDs.
            // Long-running episode counts require EP, SxxExxx or 第xxx话.
            QRegularExpression(QStringLiteral("(?<![A-Za-z0-9])E\\.?(\\d{1,2})(?![\\dA-Za-z])"), QRegularExpression::CaseInsensitiveOption),
        };
        for (int i=0;i<singles.size();++i) {
            auto matches=singles.at(i).globalMatch(text);
            while(matches.hasNext()) {
                const auto match=matches.next();
                const double n = match.captured(1).toDouble();
                if(n>0&&n<1900&&(i<3||!notEpisodes.contains(static_cast<int>(n))))
                    return {match.captured(1), false};
            }
        }
    }
    return {batch ? QStringLiteral("合集") : QString(), batch};
}

std::optional<int> parseSeason(const QString &title)
{
    static const QVector<QRegularExpression> patterns = {
        QRegularExpression(QStringLiteral("第\\s*([一二三四五六七八九十\\d]{1,3})\\s*[季期]")),
        QRegularExpression(QStringLiteral("(?<![A-Za-z0-9])S(\\d{1,2})(?=E\\d|[^\\dA-Za-z]|$)")),
        QRegularExpression(QStringLiteral("(\\d{1,2})(?:st|nd|rd|th)\\s+Season"), QRegularExpression::CaseInsensitiveOption),
        QRegularExpression(QStringLiteral("Season\\s*(\\d{1,2})(?!\\d)"), QRegularExpression::CaseInsensitiveOption),
        QRegularExpression(QStringLiteral("(?<![\\d.])(\\d)\\s*期(?!间|間)")),
    };
    QSet<int> found;
    for (const auto &pattern : patterns) {
        auto it = pattern.globalMatch(title);
        while (it.hasNext()) {
            const int n = chineseInt(it.next().captured(1));
            if (n > 0)
                found.insert(n);
        }
    }
    if (found.size() == 1)
        return *found.begin();
    return std::nullopt;
}

QStringList episodeNumbers(const QString &label)
{
    static const QRegularExpression number(QStringLiteral("^\\d{1,4}(?:\\.\\d+)?$"));
    static const QRegularExpression range(QStringLiteral("^(\\d{1,4})\\s*[-~～至]\\s*(\\d{1,4})$"));
    const QString value=label.trimmed();
    if(number.match(value).hasMatch()) {
        const double n=value.toDouble();
        return n>0 && n<1900 ? QStringList{QString::number(n,'g',12)} : QStringList{};
    }
    const auto match=range.match(value);
    QStringList out;
    if(match.hasMatch()) {
        const int first=match.captured(1).toInt(),last=match.captured(2).toInt();
        if(first>0&&last>=first&&last<1900&&last-first<1000)
            for(int i=first;i<=last;++i)out.append(QString::number(i));
    }
    return out;
}

bool isVideoFile(const QString &path)
{
    static const QSet<QString> extensions={"mkv","mp4","m4v","webm","avi","mov","ts","m2ts","mpg","mpeg","ogv","flv"};
    return extensions.contains(QFileInfo(path).suffix().toLower());
}

QStringList fileEpisodeNumbers(const QString &path)
{
    const QFileInfo file(path);
    const auto numbers=episodeNumbers(parseEpisode(file.fileName()).label);
    if(!numbers.isEmpty())return numbers;
    static const QRegularExpression plain(QStringLiteral("^(\\d{1,4}(?:\\.5)?)(?:v\\d+)?$"),QRegularExpression::CaseInsensitiveOption);
    const auto match=plain.match(file.completeBaseName());
    return match.hasMatch()?episodeNumbers(match.captured(1)):QStringList{};
}
