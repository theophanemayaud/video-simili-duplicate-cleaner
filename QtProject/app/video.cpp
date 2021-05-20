#include <QPainter>
#include "video.h"

#define FAIL_ON_FRAME_DECODE_NB_FAIL 10

Prefs Video::_prefs;
int Video::_jpegQuality = _okJpegQuality;

//DEBUGTHEO
QString ffmpeg_output="Uninitialized";

Video::Video(const Prefs &prefsParam, const QString &ffmpegPathParam, const QString &filenameParam) : filename(filenameParam), _ffmpegPath(ffmpegPathParam)
{
    _prefs = prefsParam;
    //if(_prefs._numberOfVideos > _hugeAmountVideos)       //save memory to avoid crash due to 32 bit limit
    //   _jpegQuality = _lowJpegQuality;

    if(_prefs._mainwPtr){ // during testing, mainPtr is not always defined, giving warnings : here we supress them !
        QObject::connect(this, SIGNAL(rejectVideo(Video *, QString)), _prefs._mainwPtr, SLOT(removeVideo(Video *, QString)));
        QObject::connect(this, SIGNAL(acceptVideo(Video *)), _prefs._mainwPtr, SLOT(addVideo(Video *)));
    }
}

void Video::run()
{
    if(!QFileInfo::exists(filename))
    {
        qDebug() << "Rejected : file doesn't seem to exist : "+ filename;
        emit rejectVideo(this, "file doesn't seem to exist");
        return;
    }

    Db cache(filename);
    // DDEBUGTHEO removed the cheking whether it was cached as it generated errors with all seemingly cached with height width and duration == 0
    /*if(!cache.readMetadata(*this))      //check first if video properties are cached
    {*/
        if(!getMetadata(filename))         //if not, read them with ffmpeg
            return;
        cache.writeMetadata(*this);
    //}
    if(width == 0 || height == 0 || duration == 0)
    {
        qDebug() << "Height width duration = 0 : rejected "+ filename;
        emit rejectVideo(this, "Height width duration = 0");
        return;
    }

    const int ret = takeScreenCaptures(cache);
    if(ret == _failure){
        qDebug() << "Rejected : failed to take capture : "+ filename;
        emit rejectVideo(this, "failed to take capture");
    }
    else if((_prefs._thumbnails != cutEnds && hash[0] == 0 ) ||
            (_prefs._thumbnails == cutEnds && hash[0] == 0 && hash[1] == 0)){   //all screen captures black
        qDebug() << "Rejected : all screen captures black : "+ filename;
        emit rejectVideo(this, "all screen captures black");
    }
    else{
        //qDebug() << "Accepted video "+ filename;
        emit acceptVideo(this);
    }
}

bool Video::getMetadata(const QString &filename)
{
    // Get Video stream metadata with new methods using ffmpeg library
    ffmpeg::av_log_set_level(AV_LOG_ERROR);
    ffmpeg::AVFormatContext *fmt_ctx = NULL;
    int ret;
    ret = avformat_open_input(&fmt_ctx, QDir::toNativeSeparators(filename).toStdString().c_str(), NULL, NULL);
    if (ret < 0) {
        qDebug() << "Could not open input : " + filename;
        emit rejectVideo(this, "Could not open input");
        avformat_close_input(&fmt_ctx);
        return false; // error
    }
    ret = avformat_find_stream_info(fmt_ctx, NULL);
    if (ret < 0) {
        qDebug() << "Could not find stream information : " + filename;
        emit rejectVideo(this, "Could not find stream information");
    }
    // Helpful for debugging : dumps as the executable would
//    qDebug() << "ffmpeg dump format :";
//    av_dump_format(fmt_ctx, 0, QDir::toNativeSeparators(filename).toStdString().c_str(), 0);
//    qDebug() << "\n\n";

    // Get Duration and bitrate infos (code copied from ffmpeg helper function av_dump_format see https://ffmpeg.org/doxygen/3.2/group__lavf__misc.html#gae2645941f2dc779c307eb6314fd39f10
    if (fmt_ctx->duration != AV_NOPTS_VALUE) {
        int64_t ffmpeg_duration = fmt_ctx->duration + (fmt_ctx->duration <= INT64_MAX - 5000 ? 5000 : 0);
<<<<<<< HEAD
        duration = 1000*ffmpeg_duration/AV_TIME_BASE;
=======
        duration = 1000*ffmpeg_duration/AV_TIME_BASE; // ffmpeg duration is in AV_TIME_BASE fractional time base units of seconds
        // duration is in ms, so we *1000 to go from secs to ms
>>>>>>> dccd0cc (chore(dependencies): working on getting the frame capture with the library - not working yet)
    } else {
        duration = 0;
    }
    if (fmt_ctx->bit_rate) {
      bitrate = fmt_ctx->bit_rate / 1000;
    }
    else {
        bitrate = 0;
    }

    // Get video stream information
    //      NB : previous (executable ffmpeg) code seemed to save last good video
    //          stream info if there were multiple, but now using ffmpeg find best stream
    ret = ffmpeg::av_find_best_stream(fmt_ctx,ffmpeg::AVMEDIA_TYPE_VIDEO, -1 /* auto stream selection*/,
                                      -1 /* no related stream finding*/, NULL /*no decoder return*/, 0 /* no flags*/);
//#ifdef QT_DEBUG
//    int new_rotate = 0;
//#endif
    if(ret>=0){ // Found a video stream
        ffmpeg::AVStream *vs = fmt_ctx->streams[ret]; // not necessary, but shorter for next calls
        codec = ffmpeg::avcodec_get_name(vs->codecpar->codec_id); //inspired from avcodec_string function
        width = vs->codecpar->width ; // from avcodec_parameters_to_context function -> different than vs->codec->coded_height but seems to be more correct
        height = vs->codecpar->height;
        framerate = round(ffmpeg::av_q2d(vs->avg_frame_rate) * 10) / 10;

        // handle rotated videos
        ffmpeg::AVDictionaryEntry *tag = NULL;
        tag = ffmpeg::av_dict_get(vs->metadata, "rotate", NULL /* not wanting to specify any position*/, 0 /*no flags*/);
        if(tag){
            const int rotate = QString(tag->value).toInt();
            if(rotate == 90 || rotate == 270) {
                const short temp = width;
                width = height;
                height = temp;
            }
//#ifdef QT_DEBUG
//            new_rotate = rotate; // to check with old code if equal
//#endif
        }
    }

    // Find audio stream information
    ret = ffmpeg::av_find_best_stream(fmt_ctx,ffmpeg::AVMEDIA_TYPE_AUDIO, -1 /* auto stream selection*/,
                                      -1 /* no related stream finding*/, NULL /*no decoder return*/, 0 /* no flags*/);
    if(ret>=0){ // Found a good audio stream
        ffmpeg::AVStream *as = fmt_ctx->streams[ret]; // not necessary, but shorter for next calls

        const QString audioCodec = ffmpeg::avcodec_get_name(as->codecpar->codec_id);// from avcodec_string function
        const QString rate = QString::number(as->codecpar->sample_rate);
        char buf[50];
        ffmpeg::av_get_channel_layout_string(buf, sizeof(buf), as->codecpar->channels, as->codecpar->channel_layout); // handles channel layout and number of channels !
        QString channels = buf;
        if(channels == QLatin1String("1 channels"))
            channels = QStringLiteral("mono");
        else if(channels == QLatin1String("2 channels"))
            channels = QStringLiteral("stereo");

        audio = QStringLiteral("%1 %2 Hz %3").arg(audioCodec, rate, channels);

        const int bits_per_sample = ffmpeg::av_get_bits_per_sample(as->codecpar->codec_id);
        const int bitrate = bits_per_sample ? as->codecpar->sample_rate * (int64_t)as->codecpar->channels * bits_per_sample/1000 : as->codecpar->bit_rate/1000;
        const QString kbps = QString::number(bitrate);
        if(!kbps.isEmpty() && kbps != QStringLiteral("0"))
            audio = QStringLiteral("%1 %2 kb/s").arg(audio, kbps);
    }

    avformat_close_input(&fmt_ctx);

<<<<<<< HEAD
//#ifdef QT_DEBUG
//    QProcess probe;
//    probe.setProcessChannelMode(QProcess::MergedChannels);
//    probe.setProgram(_ffmpegPath);
//    probe.setArguments(QStringList() << "-hide_banner" << "-i" << QDir::toNativeSeparators(filename));
//    probe.start();

//    probe.waitForFinished();

//    bool rotatedOnce = false;
//    int old_rotate = 0;
//    short tmp_width = 0;
//    short tmp_height = 0;

//    const QString analysis(probe.readAllStandardOutput());
////    qDebug() << "ffmpeg gave following metadata for file "<< filename;
//    const QStringList analysisLines = analysis.split(QStringLiteral("\n")); //DEBUGTHEO changed /n/r to /n (or /r/n ? Don't remember) because ffmpeg seemed only to put a \n
//    for(auto line : analysisLines)
//    {
////        qDebug() << line;
//        if(line.contains(QStringLiteral(" Duration:"))) // Keeping this temporarily but now using library FFMPEG instead of executable
//        {
//            int64_t exec_duration = 0;
//            int exec_bitrate = 0;
//            const QString time = line.split(QStringLiteral(" ")).value(3);
//            if(time == QLatin1String("N/A,"))
//                exec_duration = 0;
//            else
//            {
//                const int h  = time.midRef(0,2).toInt();
//                const int m  = time.midRef(3,2).toInt();
//                const int s  = time.midRef(6,2).toInt();
//                const int ms = time.midRef(9,2).toInt();
//                exec_duration = h*60*60*1000 + m*60*1000 + s*1000 + ms*10;
//            }
//            exec_bitrate = line.split(QStringLiteral("bitrate: ")).value(1).split(QStringLiteral(" ")).value(0).toInt();

//            if(abs(duration - exec_duration)>=10 || abs(bitrate - exec_bitrate)>=10){
//                qDebug() << "FFMPEG library resulting duration or bitrate is different from one found from executable : ";
//                qDebug() << QStringLiteral("Library duration : %1").arg(duration);
//                qDebug() << QStringLiteral("Library bitrate : %1 kb/s").arg(bitrate);

//                qDebug() << QStringLiteral("Executa duration : %1").arg(exec_duration);
//                qDebug() << QStringLiteral("Executa bitrate : %1 kb/s").arg(exec_bitrate);
//                qDebug() << " ";
//                qDebug() << " ";
//            }
//        }

//        // Get Video stream metadata : looping through all streams, so last video stream info will be saved
//        // old methods using ffmpeg executable
//        if(line.contains(QStringLiteral(" Video:")) &&
//          (line.contains(QStringLiteral("kb/s")) || line.contains(QStringLiteral(" fps")) || analysis.count(" Video:") == 1))
//        {
//            line.replace(QRegExp(QStringLiteral("\\([^\\)]+\\)")), QStringLiteral(""));
//            QString tmp_codec = line.split(QStringLiteral(" ")).value(7).replace(QStringLiteral(","), QStringLiteral(""));
//            const QString resolution = line.split(QStringLiteral(",")).value(2);
//            tmp_width = static_cast<short>(resolution.split(QStringLiteral("x")).value(0).toInt());
//            tmp_height = static_cast<short>(resolution.split(QStringLiteral("x")).value(1).split(QStringLiteral(" ")).value(0).toInt());
//            const QStringList fields = line.split(QStringLiteral(","));
//            double tmp_framerate;
//            for(const auto &field : fields)
//                if(field.contains(QStringLiteral("fps")))
//                {
//                    tmp_framerate = QStringLiteral("%1").arg(field).remove(QStringLiteral("fps")).toDouble();
//                    tmp_framerate = round(tmp_framerate * 10) / 10;     //round to one decimal point
//                }
//            // check new vs old code results
//            if(codec!=tmp_codec || framerate!=tmp_framerate || width!=tmp_width || height!=tmp_height){
//                if(!(codec==tmp_codec && framerate==tmp_framerate && (new_rotate == 90 || new_rotate == 270) && width==tmp_height && height==tmp_width)){
//                    qDebug() << "DISCREPANCY between Video stream metadata found with ffmpeg library and executable (file "<<filename<<") \n"
//                                "executa- codec:"<< tmp_codec << ", framerate:" << tmp_framerate << "fps, width x height: " << tmp_width << "x" << tmp_height << "\n"
//                                "library- codec:"<< codec << ", framerate:" << framerate << "fps, width x height: " << width << "x" << height;
//                }
//            }
//        }
//        // end of video stream metadata code old methods (to be removed once new methods are working

//        // get audio stream info -> the old way, with the executable
//        if(line.contains(QStringLiteral(" Audio:")))
//        {
//            const QString audioCodec = line.split(QStringLiteral(" ")).value(7).remove(",");
//            const QString rate = line.split(QStringLiteral(",")).value(1);
//            QString channels = line.split(QStringLiteral(",")).value(2);
//            if(channels == QLatin1String(" 1 channels"))
//                channels = QStringLiteral(" mono");
//            else if(channels == QLatin1String(" 2 channels"))
//                channels = QStringLiteral(" stereo");
//            QString tmp_audio = QStringLiteral("%1%2%3").arg(audioCodec, rate, channels);
//            const QString kbps = line.split(QStringLiteral(",")).value(4).split(QStringLiteral("kb/s")).value(0);
//            if(!kbps.isEmpty() && kbps != QStringLiteral(" 0 "))
//                tmp_audio = QStringLiteral("%1%2kb/s").arg(tmp_audio, kbps);
//            if(tmp_audio!=audio){ // this test returns lots of discrepancies, but they shouldn't be a problem as it's just text written a bit differently
//                qDebug() << "DISCREPANCY between Audio stream metadata found with ffmpeg library and executable (file "<<filename<<") \n"
//                            "executa- :"<< tmp_audio << " from line "<< line << "\n"
//                            "library- :"<< audio << "\n";
//            }
//        }

//        // check if the video stream is rotated and how, old way with the executable
//        if(line.contains(QStringLiteral("rotate")) && !rotatedOnce)
//        {
//            const int rotate = line.split(QStringLiteral(":")).value(1).toInt();
//            if(rotate == 90 || rotate == 270)
//            {
//                const short temp = tmp_width;
//                tmp_width = tmp_height;
//                tmp_height = temp;
//            }
//            rotatedOnce = true;     //rotate only once (AUDIO metadata can contain rotate keyword)
//            old_rotate = rotate;
//        }
//    }
//    if(tmp_width != width || tmp_height != height){
//        qDebug() << "DISCREPANCY in rotate between ffmpeg executable and library data : width height library="<<
//                    width << "x"<< height << " executable=" <<tmp_width<<"x"<<tmp_height<<"for file "<<filename;
//    }
//    if(old_rotate != new_rotate){ // check with library result
//        qDebug() << "DISCREPANCY between rotate video stream metadata found with ffmpeg library and executable (file "<<filename<<") \n"
//                    "executa- rotate="<< old_rotate << "\n"
//                    "library- rotate="<< new_rotate << "\n";
//    }
//#endif
=======
#ifdef QT_DEBUG
    QProcess probe;
    probe.setProcessChannelMode(QProcess::MergedChannels);
    probe.setProgram(_ffmpegPath);
    probe.setArguments(QStringList() << "-hide_banner" << "-i" << QDir::toNativeSeparators(filename));
    probe.start();

    probe.waitForFinished();

    bool rotatedOnce = false;
    int old_rotate = 0;
    const QString analysis(probe.readAllStandardOutput());
//    qDebug() << "ffmpeg gave following metadata for file "<< filename;
    const QStringList analysisLines = analysis.split(QStringLiteral("\n")); //DEBUGTHEO changed /n/r to /n (or /r/n ? Don't remember) because ffmpeg seemed only to put a \n
    for(auto line : analysisLines)
    {
//        qDebug() << line;
        if(line.contains(QStringLiteral(" Duration:"))) // Keeping this temporarily but now using library FFMPEG instead of executable
        {
            int64_t exec_duration = 0;
            int exec_bitrate = 0;
            const QString time = line.split(QStringLiteral(" ")).value(3);
            if(time == QLatin1String("N/A,"))
                exec_duration = 0;
            else
            {
                const int h  = time.midRef(0,2).toInt();
                const int m  = time.midRef(3,2).toInt();
                const int s  = time.midRef(6,2).toInt();
                const int ms = time.midRef(9,2).toInt();
                exec_duration = h*60*60*1000 + m*60*1000 + s*1000 + ms*10; // why add ms*10 ?! s*1000 -> duration is in ms but... ?!!
            }
            exec_bitrate = line.split(QStringLiteral("bitrate: ")).value(1).split(QStringLiteral(" ")).value(0).toInt();

            if(abs(duration - exec_duration)>=10 || abs(bitrate - exec_bitrate)>=10){
                qDebug() << "FFMPEG library resulting duration or bitrate is different from one found from executable : ";
                qDebug() << QStringLiteral("Library duration : %1").arg(duration);
                qDebug() << QStringLiteral("Library bitrate : %1 kb/s").arg(bitrate);

                qDebug() << QStringLiteral("Executa duration : %1").arg(exec_duration);
                qDebug() << QStringLiteral("Executa bitrate : %1 kb/s").arg(exec_bitrate);
                qDebug() << " ";
                qDebug() << " ";
            }
        }

        // Get Video stream metadata : looping through all streams, so last video stream info will be saved
        // old methods using ffmpeg executable
        if(line.contains(QStringLiteral(" Video:")) &&
          (line.contains(QStringLiteral("kb/s")) || line.contains(QStringLiteral(" fps")) || analysis.count(" Video:") == 1))
        {
            line.replace(QRegExp(QStringLiteral("\\([^\\)]+\\)")), QStringLiteral(""));
            QString tmp_codec = line.split(QStringLiteral(" ")).value(7).replace(QStringLiteral(","), QStringLiteral(""));
            const QString resolution = line.split(QStringLiteral(",")).value(2);
            short tmp_width = static_cast<short>(resolution.split(QStringLiteral("x")).value(0).toInt());
            short tmp_height = static_cast<short>(resolution.split(QStringLiteral("x")).value(1).split(QStringLiteral(" ")).value(0).toInt());
            const QStringList fields = line.split(QStringLiteral(","));
            double tmp_framerate;
            for(const auto &field : fields)
                if(field.contains(QStringLiteral("fps")))
                {
                    tmp_framerate = QStringLiteral("%1").arg(field).remove(QStringLiteral("fps")).toDouble();
                    tmp_framerate = round(tmp_framerate * 10) / 10;     //round to one decimal point
                }
            // check new vs old code results
            if(codec!=tmp_codec || framerate!=tmp_framerate || width!=tmp_width || height!=tmp_height){
                if(!(codec==tmp_codec && framerate==tmp_framerate && (new_rotate == 90 || new_rotate == 270) && width==tmp_height && height==tmp_width)){
                    qDebug() << "DISCREPANCY between Video stream metadata found with ffmpeg library and executable (file "<<filename<<") \n"
                                "executa- codec:"<< tmp_codec << ", framerate:" << tmp_framerate << "fps, width x height: " << tmp_width << "x" << tmp_height << "\n"
                                "library- codec:"<< codec << ", framerate:" << framerate << "fps, width x height: " << width << "x" << height;
                }
            }
        }
        // end of video stream metadata code old methods (to be removed once new methods are working

        // get audio stream info -> the old way, with the executable
        if(line.contains(QStringLiteral(" Audio:")))
        {
            const QString audioCodec = line.split(QStringLiteral(" ")).value(7).remove(",");
            const QString rate = line.split(QStringLiteral(",")).value(1);
            QString channels = line.split(QStringLiteral(",")).value(2);
            if(channels == QLatin1String(" 1 channels"))
                channels = QStringLiteral(" mono");
            else if(channels == QLatin1String(" 2 channels"))
                channels = QStringLiteral(" stereo");
            QString tmp_audio = QStringLiteral("%1%2%3").arg(audioCodec, rate, channels);
            const QString kbps = line.split(QStringLiteral(",")).value(4).split(QStringLiteral("kb/s")).value(0);
            if(!kbps.isEmpty() && kbps != QStringLiteral(" 0 "))
                tmp_audio = QStringLiteral("%1%2kb/s").arg(tmp_audio, kbps);
            if(tmp_audio!=audio){ // this test returns lots of discrepancies, but they shouldn't be a problem as it's just text written a bit differently
                qDebug() << "DISCREPANCY between Audio stream metadata found with ffmpeg library and executable (file "<<filename<<") \n"
                            "executa- :"<< tmp_audio << " from line "<< line << "\n"
                            "library- :"<< audio << "\n";
            }
        }

        // check if the video stream is rotated and how, old way with the executable
        if(line.contains(QStringLiteral("rotate")) && !rotatedOnce)
        {
            const int rotate = line.split(QStringLiteral(":")).value(1).toInt();
            if(rotate == 90 || rotate == 270)
            {
                const short temp = width;
                width = height;
                height = temp;
            }
            rotatedOnce = true;     //rotate only once (AUDIO metadata can contain rotate keyword)
            old_rotate = rotate;
        }
    }
    if(old_rotate != new_rotate){ // check with library result
        qDebug() << "DISCREPANCY between rotate video stream metadata found with ffmpeg library and executable (file "<<filename<<") \n"
                    "executa- rotate="<< old_rotate << "\n"
                    "library- rotate="<< new_rotate << "\n";
    }
#endif
>>>>>>> dccd0cc (chore(dependencies): working on getting the frame capture with the library - not working yet)

    const QFileInfo videoFile(filename);
    size = videoFile.size();
    modified = videoFile.lastModified();

    return true; // success !
}

int Video::takeScreenCaptures(const Db &cache)
{
    Thumbnail thumb(_prefs._thumbnails);
    QImage thumbnail(thumb.cols() * width, thumb.rows() * height, QImage::Format_RGB888);
    const QVector<int> percentages = thumb.percentages(); // percent from 1 to 100
    int capture = percentages.count();
    int ofDuration = 100; // used to "rescale" total duration... as duration*percent*ofDuration

    while(--capture >= 0)           //screen captures are taken in reverse order so errors are found early
    {
        QImage frame;
        QByteArray cachedImage = cache.readCapture(percentages[capture]);
        QBuffer captureBuffer(&cachedImage);
        bool writeToCache = false;

        // TEMPORARILY DISABLE FRAME CACHE LOADING TO TEST LIBRARY
//        if(!cachedImage.isNull())   //image was already in cache
//        {
//            frame.load(&captureBuffer, QByteArrayLiteral("JPG"));   //was saved in cache as small size, resize to original
//            frame = frame.scaled(width, height, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
//        }
//        else
//        {
            frame = captureAt(percentages[capture], ofDuration);
            if(frame.isNull())                                  //taking screen capture may fail if video is broken
            {
                ofDuration = ofDuration - _goBackwardsPercent;
                if(ofDuration >= _videoStillUsable)             //retry a few times, always closer to beginning
                {
                    capture = percentages.count();
                    continue;
                }
                return _failure;
            }
//            writeToCache = true;
//        }
        if(frame.width() > width || frame.height() > height)    //metadata parsing error or variable resolution
            return _failure;

        QPainter painter(&thumbnail);                           //copy captured frame into right place in thumbnail
        painter.drawImage(capture % thumb.cols() * width, capture / thumb.cols() * height, frame);

        if(writeToCache)
        {
            frame = minimizeImage(frame);
            frame.save(&captureBuffer, QByteArrayLiteral("JPG"), _okJpegQuality);
            cache.writeCapture(percentages[capture], cachedImage);
        }
    }

    const int hashes = _prefs._thumbnails == cutEnds? 2 : 1;    //if cutEnds mode: separate hash for beginning and end
    processThumbnail(thumbnail, hashes);
    return _success;
}

void Video::processThumbnail(QImage &thumbnail, const int &hashes)
{
    for(int hash=0; hash<hashes; hash++)
    {
        QImage image = thumbnail;
        if(_prefs._thumbnails == cutEnds)           //if cutEnds mode: separate thumbnail into first and last frames
            image = thumbnail.copy(hash*thumbnail.width()/2, 0, thumbnail.width()/2, thumbnail.height());

        cv::Mat mat = cv::Mat(image.height(), image.width(), CV_8UC3, image.bits(), static_cast<uint>(image.bytesPerLine()));
        this->hash[hash] = computePhash(mat);                           //pHash

        cv::resize(mat, mat, cv::Size(_ssimSize, _ssimSize), 0, 0, cv::INTER_AREA);
        cv::cvtColor(mat, grayThumb[hash], cv::COLOR_BGR2GRAY);
        grayThumb[hash].cv::Mat::convertTo(grayThumb[hash], CV_32F);    //ssim
    }

    thumbnail = minimizeImage(thumbnail);
    QBuffer buffer(&this->thumbnail);
    thumbnail.save(&buffer, QByteArrayLiteral("JPG"), _jpegQuality);    //save GUI thumbnail as tiny JPEG
}

uint64_t Video::computePhash(const cv::Mat &input) const
{
    cv::Mat resizeImg, grayImg, grayFImg, dctImg, topLeftDCT;
    cv::resize(input, resizeImg, cv::Size(_pHashSize, _pHashSize), 0, 0, cv::INTER_AREA);
    cv::cvtColor(resizeImg, grayImg, cv::COLOR_BGR2GRAY);           //resize image to 32x32 grayscale

    int shadesOfGray = 0;
    uchar* pixel = reinterpret_cast<uchar*>(grayImg.data);          //pointer to pixel values, starts at first one
    const uchar* lastPixel = pixel + _pHashSize * _pHashSize;
    const uchar firstPixel = *pixel;

    for(pixel++; pixel<lastPixel; pixel++)              //skip first element since that one is already firstPixel
        shadesOfGray += qAbs(firstPixel - *pixel);      //compare all pixels with first one, tabulate differences
    if(shadesOfGray < _almostBlackBitmap)
        return 0;                                       //reject video if capture was (almost) monochrome

    grayImg.convertTo(grayFImg, CV_32F);
    cv::dct(grayFImg, dctImg);                          //compute DCT (discrete cosine transform)
    dctImg(cv::Rect(0, 0, 8, 8)).copyTo(topLeftDCT);    //use only upper left 8*8 transforms (most significant ones)

    const float firstElement = *reinterpret_cast<float*>(topLeftDCT.data);      //compute avg but skip first element
    const float average = (static_cast<float>(cv::sum(topLeftDCT)[0]) - firstElement) / 63;         //(it's very big)

    uint64_t hash = 0;
    float* transform = reinterpret_cast<float*>(topLeftDCT.data);
    const float* endOfData = transform + 64;
    for(int i=0; transform<endOfData; i++, transform++)             //construct hash from all 8x8 bits
        if(*transform > average)
            hash |= 1ULL << i;                                      //larger than avg = 1, smaller than avg = 0

    return hash;
}

QImage Video::minimizeImage(const QImage &image) const
{
    if(image.width() > image.height())
    {
        if(image.width() > _thumbnailMaxWidth)
            return image.scaledToWidth(_thumbnailMaxWidth, Qt::SmoothTransformation).convertToFormat(QImage::Format_RGB888);
    }
    else if(image.height() > _thumbnailMaxHeight)
        return image.scaledToHeight(_thumbnailMaxHeight, Qt::SmoothTransformation).convertToFormat(QImage::Format_RGB888);

    return image;
}

QString Video::msToHHMMSS(const int64_t &time) const // miliseconds to user readable time string
{
    const int hours   = time / (1000*60*60) % 24;
    const int minutes = time / (1000*60) % 60;
    const int seconds = time / 1000 % 60;
    const int msecs   = time % 1000;

    QString paddedHours = QStringLiteral("%1").arg(hours);
    if(hours < 10)
        paddedHours = QStringLiteral("0%1").arg(paddedHours);

    QString paddedMinutes = QStringLiteral("%1").arg(minutes);
    if(minutes < 10)
        paddedMinutes = QStringLiteral("0%1").arg(paddedMinutes);

    QString paddedSeconds = QStringLiteral("%1").arg(seconds);
    if(seconds < 10)
        paddedSeconds = QStringLiteral("0%1").arg(paddedSeconds);

    return QStringLiteral("%1:%2:%3.%4").arg(paddedHours, paddedMinutes, paddedSeconds).arg(msecs);
}

QImage Video::captureAt(const int &percent, const int &ofDuration) const
{
    const QTemporaryDir tempDir;
    if(!tempDir.isValid())
        return QImage();

    /*const*/ QString screenshot = QStringLiteral("%1/duplicate%2.bmp").arg(tempDir.path()).arg(percent);

#ifdef QT_DEBUG
    // test output path
    screenshot = QStringLiteral("/Users/theophanemayaud/Downloads/Testvids/%1-%2prct-lib.jpg").arg(QFileInfo(filename).completeBaseName()).arg(percent);

    qDebug() << "\n";
    ffmpegLib_captureAt(screenshot, percent, ofDuration);
//    if(!ffmpegLib_captureAt(screenshot, percent, ofDuration))
//        qDebug() << "Failed to do caputre with library for " << filename;
#endif

#ifdef QT_DEBUG
    // test output path
    screenshot = QStringLiteral("/Users/theophanemayaud/Downloads/Testvids/%1-%2prct-exec.bmp").arg(QFileInfo(filename).completeBaseName()).arg(percent);
#endif
    QProcess ffmpeg;
    ffmpeg.setProgram(_ffmpegPath);
    qDebug() << "Executable seeking to timestamp : " << msToHHMMSS(duration * (percent * ofDuration) / (100 * 100)) << " for percent="<< percent << " ofDuration="<< ofDuration;
    qDebug() << "\n";
    ffmpeg.setArguments(QStringList() <<
                        "-ss" << msToHHMMSS(duration * (percent * ofDuration) / (100 * 100)) <<
                        "-i" << QDir::toNativeSeparators(filename) << "-an" << "-frames:v" << "1" <<
                        "-pix_fmt" << "rgb24" <<  QDir::toNativeSeparators(screenshot) );
    ffmpeg.start();
    ffmpeg.waitForFinished(10000);

    const QImage img(screenshot, "BMP");
    //QFile::remove(screenshot);

    return img;
}

bool Video::ffmpegLib_captureAt(const QString imgPathname, const int percent, const int ofDuration) const
{
    // new ffmpeg library code to capture an image :
    // inspired from http://blog.allenworkspace.net/2020/06/ffmpeg-decode-and-then-encode-frames-to.html
    // general understanding https://github.com/leandromoreira/ffmpeg-libav-tutorial
    // qImage from avframe : https://stackoverflow.com/questions/13088749/efficient-conversion-of-avframe-to-qimage

    ffmpeg::AVFormatContext *fmt_ctx = NULL;

    /* open input file, and allocate format context */
    if (ffmpeg::avformat_open_input(&fmt_ctx, QDir::toNativeSeparators(filename).toStdString().c_str(), NULL, NULL) < 0) {
        qDebug() << "Could not open source file " << filename;
        avformat_close_input(&fmt_ctx);
        return false;
    }

    /* retrieve stream information */
    if (ffmpeg::avformat_find_stream_info(fmt_ctx, NULL) < 0) {
        qDebug() << "Could not find stream information" << filename;
        avformat_close_input(&fmt_ctx);
        return false;
    }

    // find best video stream
    const int stream_index = ffmpeg::av_find_best_stream(fmt_ctx,ffmpeg::AVMEDIA_TYPE_VIDEO,
                                                         -1 /* auto stream selection*/,
                                                         -1 /* no related stream finding*/,
                                                         NULL /*don't find decoder*/,
                                                         0 /* no flags*/);
    if(stream_index<0){ // Did not find a video stream
        qDebug() << "Could not find a good video stream" << filename;
        ffmpeg::avformat_close_input(&fmt_ctx);
        return false;
    }
    ffmpeg::AVStream *vs = fmt_ctx->streams[stream_index]; // set shorter reference to video stream of interest
#ifdef QT_DEBUG // av_find_best_stream should not return non video stream if video stream requested !
    if(vs->codecpar->codec_type!=ffmpeg::AVMEDIA_TYPE_VIDEO){
        qDebug() << "FFMPEG returned stream was not video... !" << filename;
        ffmpeg::avformat_close_input(&fmt_ctx);
        return false;
    }
#endif
    /* find decoder for the stream */
    ffmpeg::AVCodec *codec = ffmpeg::avcodec_find_decoder(vs->codecpar->codec_id); // null if none found
    if (!codec) {
        qDebug() << "Failed to find video codec "<< ffmpeg::avcodec_get_name(vs->codecpar->codec_id) << " for file " << filename;
        ffmpeg::avformat_close_input(&fmt_ctx);
        return false;
    }

    /* Allocate a codec context for the decoder */
    ffmpeg::AVCodecContext *codec_ctx = ffmpeg::avcodec_alloc_context3(codec);
    if (!codec_ctx) {
        qDebug() << "Failed to allocate the video codec context for file " << filename;
        // NB here codec context failed to alloc, but next steps will need to free it !
        ffmpeg::avformat_close_input(&fmt_ctx);
        return false;
    }

    /* Copy codec parameters from input stream to codec context */
    if (ffmpeg::avcodec_parameters_to_context(codec_ctx, vs->codecpar) < 0) {
        qDebug() << "Failed to copy codec parameters to decoder context for file" << filename;
        ffmpeg::avcodec_free_context(&codec_ctx);
        ffmpeg::avformat_close_input(&fmt_ctx);
        return false;
    }

    /* Open the codec */
    if(ffmpeg::avcodec_open2(codec_ctx, codec, NULL /* no options*/)<0){
       qDebug() << "Failed to open video codec for file " << filename;
       ffmpeg::avcodec_free_context(&codec_ctx);
       ffmpeg::avformat_close_input(&fmt_ctx);
       return false;
    }

    /* Allocate packet for storing decoded data */
    ffmpeg::AVPacket* vPacket = ffmpeg::av_packet_alloc();
    if (!vPacket)
    {
        qDebug() << "failed to allocated memory for AVPacket for file " << filename;
        //ffmpeg::av_packet_free(&vPacket); //NB failed to alloc, so don't call yet but next time
        ffmpeg::avcodec_free_context(&codec_ctx);
        ffmpeg::avformat_close_input(&fmt_ctx);
        return false;
    }

    /* Allocate a frame */
    ffmpeg::AVFrame* vFrame = ffmpeg::av_frame_alloc();
    if (!vFrame){
        qDebug() << "failed to allocated memory for frame for file " << filename;
        //ffmpeg::av_frame_free(&vFrame); // NB failed to alloc, so don't call yet but next time
        ffmpeg::av_packet_free(&vPacket);
        ffmpeg::avcodec_free_context(&codec_ctx);
        ffmpeg::avformat_close_input(&fmt_ctx);
        return false;
    }

    // Now let's find and read the packets, then frame
    bool readFrame = false;
    int failedFrames = 0;
    // get timestamp, in units of stream time base. We have in miliseconds
    long long at_ms_percent = duration * (percent * ofDuration) /(100 * 100 /*percents*/);
    int64_t wanted_ts = vs->time_base.den * at_ms_percent / (1000 /*we have ms*/ * vs->time_base.num);
    qDebug() << "Seeking to timestamp " << msToHHMMSS(duration * (percent * ofDuration) /
                                                      (100 * 100))<< " for file (of duration " << msToHHMMSS(duration) << " ) " << filename;
    qDebug() << "In terms of ffmpeg units : "<< wanted_ts << " for stream duration " << vs->duration ;

    if(ffmpeg::av_seek_frame(fmt_ctx, stream_index, wanted_ts, AVSEEK_FLAG_BACKWARD /* 0 for no flags*/) < 0){ // will seek to closest previous keyframe
//    if(ffmpeg::avformat_seek_file(fmt_ctx, stream_index, INT64_MIN, wanted_ts, wanted_ts, 0/* no flags*/) < 0){
        qDebug() << "failed to seek to frame at timestamp " << msToHHMMSS(1000*wanted_ts*vs->time_base.num/vs->time_base.den) << " for file " << filename;
        ffmpeg::av_packet_free(&vPacket);
        ffmpeg::av_frame_free(&vFrame);
        ffmpeg::avcodec_free_context(&codec_ctx);
        ffmpeg::avformat_close_input(&fmt_ctx);
        return false;
    }
    while (readFrame == false){
        if(ffmpeg::av_read_frame(fmt_ctx, vPacket) /* reads all stream frames, must check for when video*/ < 0){
            failedFrames++;
            if(failedFrames>FAIL_ON_FRAME_DECODE_NB_FAIL){
                qDebug() << "failed to read frame for file " << filename;
                ffmpeg::av_packet_free(&vPacket);
                ffmpeg::av_frame_free(&vFrame);
                ffmpeg::avcodec_free_context(&codec_ctx);
                ffmpeg::avformat_close_input(&fmt_ctx);
                return false;
            }
            continue;
        }
        failedFrames = 0;

        // if it's the video stream
        if (vPacket->stream_index == stream_index) {
            if (ffmpeg::avcodec_send_packet(codec_ctx, vPacket) < 0)
            {
                qDebug() << "failed to send raw packet to decoder for file " << filename;
                ffmpeg::av_packet_free(&vPacket);
                ffmpeg::av_frame_free(&vFrame);
                ffmpeg::avcodec_free_context(&codec_ctx);
                ffmpeg::avformat_close_input(&fmt_ctx);
                return false;
            }
            if(ffmpeg::avcodec_receive_frame(codec_ctx, vFrame) == 0){ // should debug codes, 0 means success, otherwise just try again
#ifdef QT_DEBUG
                qDebug() << "Success reading frame " <<
                            "type "<< av_get_picture_type_char(vFrame->pict_type) <<
                            " frame number " << codec_ctx->frame_number <<
                            " pts "<< vFrame->pts <<
                            " for file " << filename;
#endif
                if((wanted_ts-vFrame->pts)<=0){ // must read frames until we get frame at wanted timestamp
                    if(!saveToJPEG(vFrame, imgPathname )){
                        qDebug() << "Failed to save img for file " << filename;
                        ffmpeg::av_packet_free(&vPacket);
                        ffmpeg::av_frame_free(&vFrame);
                        ffmpeg::avcodec_free_context(&codec_ctx);
                        ffmpeg::avformat_close_input(&fmt_ctx);
                        return false;
                    }
                    readFrame = true;
                }
            }
        }
        av_packet_unref(vPacket);
        // Limit the number of output frame to be 5.
    }

    ffmpeg::av_packet_free(&vPacket);
    ffmpeg::av_frame_free(&vFrame);
    ffmpeg::avcodec_free_context(&codec_ctx);
    ffmpeg::avformat_close_input(&fmt_ctx);

    qDebug() << "Library capture success at timestamp " << msToHHMMSS(1000*wanted_ts*vs->time_base.num/vs->time_base.den) << " for file " << filename;
    return true;
}

bool Video::saveToJPEG(const ffmpeg::AVFrame* pFrame, const QString imgPathname) const//const char * folderName, int index)
{
    FILE *f;
    int i;
    f = fopen(imgPathname.toStdString().c_str(),"w");
    // writing the minimal required header for a pgm file format
    // portable graymap format -> https://en.wikipedia.org/wiki/Netpbm_format#PGM_example
    fprintf(f, "P5\n%d %d\n%d\n", pFrame->width, pFrame->height, 255);

    // writing line by line
    for (i = 0; i < pFrame->height; i++)
        fwrite(pFrame->data[0] + i * pFrame->linesize[0], 1, pFrame->width, f);
    fclose(f);
    return true;
}

/*
bool Video::saveToJPEG(const ffmpeg::AVFrame* pFrame, const QString imgPathname) const//const char * folderName, int index)
{
    ffmpeg::av_log_set_level(AV_LOG_VERBOSE);

    ffmpeg::AVFormatContext* pFormatCtx = ffmpeg::avformat_alloc_context();
    if(pFormatCtx == NULL){
        qDebug() << "Couldn't allocate format context for output " << imgPathname;
        return false;
    }

    // Setup the output format
    pFormatCtx->oformat = ffmpeg::av_guess_format("mjpeg", NULL, NULL);
    if(pFormatCtx->oformat == NULL){
        qDebug() << "Couldn't find output format for pathname " << imgPathname;
        ffmpeg::avformat_free_context(pFormatCtx);
        return false;
    }

    // TODO : check if image file already exists and fail !
    // Initialize output file
    if (ffmpeg::avio_open(&pFormatCtx->pb, imgPathname.toStdString().c_str(), AVIO_FLAG_READ_WRITE) < 0) {
        qDebug() << "Couldn't open output file with pathname " << imgPathname;
        ffmpeg::avformat_free_context(pFormatCtx);
        return false;
    }

    // Get a new Stream from the indicated format context
    ffmpeg::AVStream* pAVStream = ffmpeg::avformat_new_stream(pFormatCtx, 0);
    if (pAVStream == NULL) {
        qDebug() << "Couldn't get new stream " << imgPathname;
        ffmpeg::avformat_free_context(pFormatCtx);
        ffmpeg::avio_close(pFormatCtx->pb);
        return false;
    }

    // Find encoder from the codec identifier.
    ffmpeg::AVCodec* pCodec = ffmpeg::avcodec_find_encoder(pFormatCtx->oformat->video_codec);
    if (pCodec == NULL) {
        qDebug() << "Codec not found for " << imgPathname;
        ffmpeg::avformat_free_context(pFormatCtx);
        return false;
    }

    // Setup the codec context
    ffmpeg::AVCodecContext* codecCtx = ffmpeg::avcodec_alloc_context3(pCodec);
    if(codecCtx == NULL){
        ffmpeg::avformat_free_context(pFormatCtx);
        qDebug() << "Couldn't allocate ouput codec " << imgPathname;
        return false;
    }
    codecCtx->codec_id =  pFormatCtx->oformat->video_codec;
    codecCtx->codec_type = ffmpeg::AVMEDIA_TYPE_VIDEO;
    codecCtx->pix_fmt = ffmpeg::AV_PIX_FMT_YUVJ420P;
    codecCtx->width = pFrame->width;
    codecCtx->height = pFrame->height;
    codecCtx->time_base = ffmpeg::AVRational{ 1, 25 };
    qDebug() << "We have a video height="<<pFrame->height<<" width="<<pFrame->width;

    // Open the codec
//    ffmpeg::AVDictionary *options = NULL;
//    ffmpeg::av_dict_set(&options, "pixel_format", "rgb24", 0); // try do do rgb24 as before
    if (ffmpeg::avcodec_open2(codecCtx, pCodec, NULL) < 0) {
        ffmpeg::avformat_free_context(pFormatCtx);
        qDebug() << "Could not open output codec for " << imgPathname;
        return false;
    }

    // assign the codec context to the stream parameters.
    if(ffmpeg::avcodec_parameters_from_context(pAVStream->codecpar, codecCtx)<0){
        qDebug() << "Could copy codec params for " << imgPathname;
        return false;
    }

    //Write Header
    if(ffmpeg::avformat_write_header(pFormatCtx, NULL)<0){
        qDebug() << "Could not write output header for " << imgPathname;
        return false;
    }

    int y_size = (codecCtx->width) * (codecCtx->height);

    // assign large enough space
    ffmpeg::AVPacket pkt;
    if(ffmpeg::av_new_packet(&pkt, y_size)<0){
        qDebug() << "Could not allocate packet for " << imgPathname;
        return false;
    }

    // Send input video frame to the image encoder, read it, and write it
    if (ffmpeg::avcodec_send_frame(codecCtx, pFrame) < 0) {
        qDebug() << "Encode Error for output " << imgPathname;
        return false;
    }
    if(ffmpeg::avcodec_receive_packet(codecCtx, &pkt)<0){
        qDebug() << "Receive packet error for output " << imgPathname;
        return false;
    }
    if(ffmpeg::av_write_frame(pFormatCtx, &pkt)<0){
        qDebug() << "Write frame error for output " << imgPathname;
        return false;
    }

    ffmpeg::av_packet_unref(&pkt);

    //Write Trailer
    if(ffmpeg::av_write_trailer(pFormatCtx)<0){
        qDebug() << "Failed to write trailer for output " << imgPathname;
        return false;
    }

    qDebug() << "Encode Successful for ouptut " << imgPathname;

    ffmpeg::avcodec_close(codecCtx);

    ffmpeg::avio_close(pFormatCtx->pb);

    ffmpeg::avformat_free_context(pFormatCtx);
    ffmpeg::avcodec_free_context(&codecCtx);

    return true;
}
*/
/*bool Video::saveToJPEG(const ffmpeg::AVFrame* pFrame, const QString imgPathname, const ffmpeg::AVPixelFormat pix_fmt) const//const char * folderName, int index)
{
    ffmpeg::AVCodec *jpegCodec = ffmpeg::avcodec_find_encoder(ffmpeg::AV_CODEC_ID_MJPEG);
    if (!jpegCodec) {
        qDebug() << "Error finding mjpeg encoder with output img "<< imgPathname;
        return false;
    }
    ffmpeg::AVCodecContext *jpegContext = avcodec_alloc_context3(jpegCodec);
    if (!jpegContext) {
        qDebug() << "Error allocating context with output img "<< imgPathname;
        return false;
    }

    qDebug() << "We have a video height="<<pFrame->height<<" width="<<pFrame->width;
    jpegContext->pix_fmt = ffmpeg::AV_PIX_FMT_YUVJ420P;
    jpegContext->height = pFrame->height;
    jpegContext->width = pFrame->width;
    jpegContext->time_base = ffmpeg::AVRational{ 1, 25 };

    if (ffmpeg::avcodec_open2(jpegContext, jpegCodec, NULL) < 0) {
        qDebug() << "Error opening codec with output img "<< imgPathname;
        return false;
    }
    FILE *JPEGFile;

    ffmpeg::AVPacket packet = {.data = NULL, .size = 0};
    ffmpeg::av_init_packet(&packet);
    int gotFrame;

    if (ffmpeg::avcodec_encode_video2(jpegContext, &packet, pFrame, &gotFrame) < 0) {
        qDebug() << "Error encoding mjpeg with output img "<< imgPathname;
        return false;
    }

    JPEGFile = fopen(imgPathname.toStdString().c_str(), "wb");
    fwrite(packet.data, 1, packet.size, JPEGFile);
    fclose(JPEGFile);

    ffmpeg::av_free_packet(&packet);
    ffmpeg::avcodec_close(jpegContext);
    return true;
}*/
