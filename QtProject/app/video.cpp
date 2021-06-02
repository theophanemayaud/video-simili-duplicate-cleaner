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
        duration = 1000*ffmpeg_duration/AV_TIME_BASE; // ffmpeg duration is in AV_TIME_BASE fractional time base units of seconds
        // duration is in ms, so we *1000 to go from secs to ms
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
            _rotateAngle = QString(tag->value).toInt();
            if(_rotateAngle == 90 || _rotateAngle == 270) {
                const short temp = width;
                width = height;
                height = temp;
            }
//#ifdef QT_DEBUG
//            new_rotate = _rotateAngle; // to check with old code if equal
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

        if(!cachedImage.isNull())   //image was already in cache
        {
            frame.load(&captureBuffer, QByteArrayLiteral("JPG"));   //was saved in cache as small size, resize to original
            frame = frame.scaled(width, height, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        }
        else
        {
            frame = captureAt(percentages[capture], ofDuration);
            if(frame.isNull())                                  //taking screen capture may fail if video is broken
            {
                ofDuration = ofDuration - _goBackwardsPercent;
                if(ofDuration >= _videoStillUsable)             //retry a few times, always closer to beginning
                {
                    capture = percentages.count();
                    continue;
                }
                qDebug() << "Failing because not enough video seems useable at "<< percentages[capture]<< "% ofdur "<< ofDuration << " for "<<filename;
                return _failure;
            }
            writeToCache = true;
        }
        if(frame.width() > width || frame.height() > height){    //metadata parsing error or variable resolution
            qDebug() << "Failing because capture height="<<frame.height()<<",width="<<frame.width()<<" is different to vid metadata height="<<width<<",width="<<height<< "for file "<< filename;
            return _failure;
        }

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

    QString paddedMSeconds = QStringLiteral("%1").arg(msecs);
    if(msecs < 10)
        paddedMSeconds = QStringLiteral("00%1").arg(paddedMSeconds);
    else if(msecs < 100)
        paddedMSeconds = QStringLiteral("0%1").arg(paddedMSeconds);


    return QStringLiteral("%1:%2:%3.%4").arg(paddedHours, paddedMinutes, paddedSeconds).arg(paddedMSeconds);
}

QImage Video::captureAt(const int &percent, const int &ofDuration) const
{
    const QTemporaryDir tempDir;
    if(!tempDir.isValid())
        return QImage();

    const QString screenshotPath = QStringLiteral("%1/duplicate%2.bmp").arg(tempDir.path()).arg(percent);

    const QImage libImg = ffmpegLib_captureAt(screenshotPath, percent, ofDuration);

    return libImg;
}

QImage Video::ffmpegLib_captureAt(const QString imgPathname, const int percent, const int ofDuration) const
{
//    // NB Here a lot of comments are kept as is, because they quickly enable diagnstics of new or problematic
//    // video codecs/files/...
//    qDebug() << "Library will try to capture at "<<percent<<"% ofduration "<<ofDuration<<"% for file "<< filename;
    ffmpeg::av_log_set_level(AV_LOG_FATAL);
    // new ffmpeg library code to capture an image :
    // inspired from http://blog.allenworkspace.net/2020/06/ffmpeg-decode-and-then-encode-frames-to.html
    // general understanding https://github.com/leandromoreira/ffmpeg-libav-tutorial
    // qImage from avframe : https://stackoverflow.com/questions/13088749/efficient-conversion-of-avframe-to-qimage
    QImage img;

    ffmpeg::AVFormatContext *fmt_ctx = NULL;

    /* open input file, and allocate format context */
    if (ffmpeg::avformat_open_input(&fmt_ctx, QDir::toNativeSeparators(filename).toStdString().c_str(), NULL, NULL) < 0) {
        qDebug() << "Could not open source file " << filename;
        avformat_close_input(&fmt_ctx);
        return img;
    }

    /* retrieve stream information */
    if (ffmpeg::avformat_find_stream_info(fmt_ctx, NULL) < 0) {
        qDebug() << "Could not find stream information" << filename;
        avformat_close_input(&fmt_ctx);
        return img;
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
        return img;
    }
    ffmpeg::AVStream *vs = fmt_ctx->streams[stream_index]; // set shorter reference to video stream of interest
    // av_find_best_stream should not return non video stream if video stream requested !
    if(vs->codecpar->codec_type!=ffmpeg::AVMEDIA_TYPE_VIDEO){
        qDebug() << "FFMPEG returned stream was not video... !" << filename;
        ffmpeg::avformat_close_input(&fmt_ctx);
        return img;
    }

    /* find decoder for the stream */
    ffmpeg::AVCodec *codec = ffmpeg::avcodec_find_decoder(vs->codecpar->codec_id); // null if none found
    if (!codec) {
        qDebug() << "Failed to find video codec "<< ffmpeg::avcodec_get_name(vs->codecpar->codec_id) << " for file " << filename;
        ffmpeg::avformat_close_input(&fmt_ctx);
        return img;
    }

    /* Allocate a codec context for the decoder */
    ffmpeg::AVCodecContext *codec_ctx = ffmpeg::avcodec_alloc_context3(codec);
    if (!codec_ctx) {
        qDebug() << "Failed to allocate the video codec context for file " << filename;
        // NB here codec context failed to alloc, but next steps will need to free it !
        ffmpeg::avformat_close_input(&fmt_ctx);
        return img;
    }

    /* Copy codec parameters from input stream to codec context */
    if (ffmpeg::avcodec_parameters_to_context(codec_ctx, vs->codecpar) < 0) {
        qDebug() << "Failed to copy codec parameters to decoder context for file" << filename;
        ffmpeg::avcodec_free_context(&codec_ctx);
        ffmpeg::avformat_close_input(&fmt_ctx);
        return img;
    }

    /* Open the codec */
    if(ffmpeg::avcodec_open2(codec_ctx, codec, NULL /* no options*/)<0){
       qDebug() << "Failed to open video codec for file " << filename;
       ffmpeg::avcodec_free_context(&codec_ctx);
       ffmpeg::avformat_close_input(&fmt_ctx);
       return img;
    }

    /* Allocate packet for storing decoded data */
    ffmpeg::AVPacket* vPacket = ffmpeg::av_packet_alloc();
    if (!vPacket){
        qDebug() << "failed to allocated memory for AVPacket for file " << filename;
        //ffmpeg::av_packet_free(&vPacket); //NB failed to alloc, so don't call yet but next time
        ffmpeg::avcodec_free_context(&codec_ctx);
        ffmpeg::avformat_close_input(&fmt_ctx);
        return img;
    }

    /* Allocate a frame */
    ffmpeg::AVFrame* vFrame = ffmpeg::av_frame_alloc();
    if (!vFrame){
        qDebug() << "failed to allocated memory for frame for file " << filename;
        //ffmpeg::av_frame_free(&vFrame); // NB failed to alloc, so don't call yet but next time
        ffmpeg::av_packet_free(&vPacket);
        ffmpeg::avcodec_free_context(&codec_ctx);
        ffmpeg::avformat_close_input(&fmt_ctx);
        return img;
    }

    // Now let's find and read the packets, then frame
    bool readFrame = false;
    int failedFrames = 0;
    // get timestamp, in units of stream time base. We have in miliseconds
    int64_t ms_stream_duration = 1000*vs->duration*vs->time_base.num/vs->time_base.den;
    // TODO : we use stream duration here, not duration gotten from metadata,
    //        but sometimes metadata file duration is very different => should find a way to report it
    if(abs(ms_stream_duration-duration)>100){
//        qDebug() << "Stream duration ms(" << ms_stream_duration << ") is more than 100ms different to file reported duration("<<duration<<") (we're using video stream duration for captures) " << filename;
    }
    const int64_t wanted_ms_place = (double) ms_stream_duration *  percent * ofDuration / (100 * 100 /*percents*/) ;
    const int64_t ts_diff = vs->avg_frame_rate.den * vs->time_base.den / (vs->avg_frame_rate.num * vs->time_base.num);
    long long start_time=0;
    if(vs->start_time!=AV_NOPTS_VALUE){
        start_time = vs->start_time;
    }
    // don't need to offset by vs->start_time as stream duration doesn't include this offset
    const int64_t wanted_ts = vs->time_base.den * wanted_ms_place / (vs->time_base.num * 1000 /*we have ms duration*/);

//    qDebug() << "Will try to get frame at "<<percent<<"% ofdur "<<ofDuration<<"% for "<< filename;
//    qDebug() << "wanted_ms_place "<<wanted_ms_place << " video stream ms duration "<< ms_stream_duration;
//    qDebug() << "ts_diff_between_images "<< ts_diff << " in terms of time "<< msToHHMMSS(1000*ts_diff*vs->time_base.num/vs->time_base.den);
//    qDebug() << "tb num "<<vs->time_base.num<<" den "<<vs->time_base.den;
//    qDebug() << "avg fps="<<(float)vs->avg_frame_rate.num/vs->avg_frame_rate.den;
//    qDebug() << "start pts="<< start_time << "in terms of time "<< msToHHMMSS(1000*start_time*vs->time_base.num/vs->time_base.den);
//    qDebug() << "stream duration="<< vs->duration <<
//                " in terms of time " << msToHHMMSS(ms_stream_duration) <<
//                " in terms of ms "<< ms_stream_duration;

//    qDebug() << "Seeking to timestamp " <<
//                msToHHMMSS(ms_stream_duration * (percent * ofDuration) / (100 * 100))<<
//                " for file (of duration " << msToHHMMSS(ms_stream_duration) << " ) " << filename;
//    qDebug() << "In terms of ffmpeg units : "<< wanted_ts << " for stream duration " << vs->duration ;

    // From http://ffmpeg.org/ffmpeg.html#Main-options see -ss option (as was previously used with executable)
    // "ffmpeg will seek to the closest seek point before position"
    // -> need to seek to closest previous keyframe and then decode each frame until the one right BEFORE (or at/after) exact position
    // in fact from manual tests with the executable, it seems the behavior was not very consistent. Will just seek to frame right before or after timestamp
    if(ffmpeg::av_seek_frame(fmt_ctx, stream_index, wanted_ts, AVSEEK_FLAG_BACKWARD /* 0 for no flags*/) < 0){ // will seek to closest previous keyframe
        qDebug() << "failed to seek to frame at timestamp " << msToHHMMSS(1000*wanted_ts*vs->time_base.num/vs->time_base.den) << " for file " << filename;
        ffmpeg::av_packet_free(&vPacket);
        ffmpeg::av_frame_free(&vFrame);
        ffmpeg::avcodec_free_context(&codec_ctx);
        ffmpeg::avformat_close_input(&fmt_ctx);
        return img;
    }
    while (readFrame == false){
        int ret = ffmpeg::av_read_frame(fmt_ctx, vPacket); // reads packet frames, but buffers some so at the end,
                                                            // even if it says EOF we need to decode again until there's nothing returned
        if(ret == AVERROR_EOF){
//            qDebug() << "av_read_frame said eof for file " << filename;
        }
        if( ret /* reads all stream frames, must check for when video*/ < 0){
            failedFrames++;
//            qDebug() << "Failed to read 1 frame, will try again for file" << filename;
            if(failedFrames>FAIL_ON_FRAME_DECODE_NB_FAIL){
                qDebug() << "Failed to read frames too many("<< failedFrames << ") times, will stop, for file " << filename;
                ffmpeg::av_packet_free(&vPacket);
                ffmpeg::av_frame_free(&vFrame);
                ffmpeg::avcodec_free_context(&codec_ctx);
                ffmpeg::avformat_close_input(&fmt_ctx);
                return img;
            }
            if(ret != AVERROR_EOF){
                continue;
            }
        }
        else{
            failedFrames = 0;
        }

        // if it's the video stream
        if (vPacket->stream_index == stream_index) {
            if (ffmpeg::avcodec_send_packet(codec_ctx, vPacket) < 0)
            {
                if(ret == AVERROR_EOF){
                    qDebug() << "Reached end of file but no matching frame for " << filename;
                }
                else{
                    qDebug() << "Failed to send raw packet to decoder for file " << filename;
                }
                ffmpeg::av_packet_free(&vPacket);
                ffmpeg::av_frame_free(&vFrame);
                ffmpeg::avcodec_free_context(&codec_ctx);
                ffmpeg::avformat_close_input(&fmt_ctx);
                return img;
            }
            if(ffmpeg::avcodec_receive_frame(codec_ctx, vFrame) == 0){ // should debug codes, 0 means success, otherwise just try again
                long long curr_ts = -1; // if no other value, we'll effectively skip that frame, except if wanting the first frame and that's the first one we get
                if(vFrame->pts>=0){
                    curr_ts = vFrame->pts;
                }
                else if(vPacket->pts>=0){
                    curr_ts = vPacket->pts;
                }
                else if(vFrame->pkt_dts>=0){
                    curr_ts = vFrame->pkt_dts;
                }

//                saveToJPEG(codec_ctx, vFrame, imgPathname +  " pts" + msToHHMMSS((long long)1000*vFrame->pts*vs->time_base.num/vs->time_base.den) + ".bmp");
//                qDebug() << "Frame " <<
//                            "type "<< av_get_picture_type_char(vFrame->pict_type) <<
//                            " frame number " << codec_ctx->frame_number <<
//                            " wanted ts (from start)"<<wanted_ts<<" in time (from 0)"<<msToHHMMSS(1000*(wanted_ts-start_time)*vs->time_base.num/vs->time_base.den)<<
//                            " pts "<<vFrame->pts << " in time (from0)"<< msToHHMMSS(1000*(vFrame->pts-start_time)*vs->time_base.num/vs->time_base.den) <<
//                            " pkt dts " << vFrame->pkt_dts<<
//                            " curr_ts "<<curr_ts
//                if(curr_ts==-1){
//                    qDebug()<<"Problem with pts and dts (=-1) with file " << filename;
//                }

                if((curr_ts-wanted_ts)>=-ts_diff){ // must read frames until we get before wanted time stamp
                    img = getQImageFromFrame(codec_ctx, vFrame, imgPathname);
                    if(img.isNull()){
                        qDebug() << "Failed to save img for file " << filename;
                        ffmpeg::av_packet_free(&vPacket);
                        ffmpeg::av_frame_free(&vFrame);
                        ffmpeg::avcodec_free_context(&codec_ctx);
                        ffmpeg::avformat_close_input(&fmt_ctx);
                        return QImage();
                    }
                    readFrame = true;
//                    qDebug() << "READ FRAME";
                }
            }
            else{
//                qDebug() << "Failed to receive frame for "<<filename;
            }
        }
        else{
//            qDebug() << "Packet not of wanted stream for "<<filename;
            if(ret == AVERROR_EOF){
                qDebug() << "Reached end of file, no more stream packets, found no matching frame for " << filename;
                ffmpeg::av_packet_free(&vPacket);
                ffmpeg::av_frame_free(&vFrame);
                ffmpeg::avcodec_free_context(&codec_ctx);
                ffmpeg::avformat_close_input(&fmt_ctx);
                return img;
            }
        }
        ffmpeg::av_packet_unref(vPacket);
    }

    ffmpeg::av_packet_free(&vPacket);
    ffmpeg::av_frame_free(&vFrame);
    ffmpeg::avcodec_free_context(&codec_ctx);
    ffmpeg::avformat_close_input(&fmt_ctx);

    if(img.isNull()){
        qDebug() << "ERROR with taking frame function, it is empty but shouldn't be !!! "<<filename;
    }
//    else{
//    qDebug() << "Library capture success at timestamp " << msToHHMMSS(1000*wanted_ts*vs->time_base.num/vs->time_base.den) << " for file " << filename;
//    }
    return img;
}

QImage Video::getQImageFromFrame(const ffmpeg::AVCodecContext *codec_ctx, const ffmpeg::AVFrame* pFrame, const QString imgPathname) const//const char * folderName, int index)
{
    // first convert frame to rgb24
    ffmpeg::SwsContext* img_convert_ctx;
    img_convert_ctx = sws_getContext(codec_ctx->width,
                                     codec_ctx->height,
                                     codec_ctx->pix_fmt,
                                     codec_ctx->width,
                                     codec_ctx->height,
                                     ffmpeg::AV_PIX_FMT_RGB24,
                                     SWS_BICUBIC, NULL, NULL, NULL);
    ffmpeg::AVFrame* frameRGB = ffmpeg::av_frame_alloc();
    ffmpeg::avpicture_alloc((ffmpeg::AVPicture*)frameRGB,
                            ffmpeg::AV_PIX_FMT_RGB24,
                            codec_ctx->width,
                            codec_ctx->height);

    sws_scale(img_convert_ctx,
              pFrame->data,
              pFrame->linesize, 0,
              codec_ctx->height,
              frameRGB->data,
              frameRGB->linesize);
    QImage image(frameRGB->data[0],
                     codec_ctx->width,
                     codec_ctx->height,
                     frameRGB->linesize[0],
                     QImage::Format_RGB888);

    // if the video had rotated metadata, it needs to be flipped !
    if(_rotateAngle%360 == 90){
        image = image.transformed(QTransform().rotate(90.0));
    }
    else if(_rotateAngle%360 == 180){
        image = image.transformed(QTransform().rotate(180.0));
    }
    else if(_rotateAngle%360 == 270){
        image = image.transformed(QTransform().rotate(270.0));
    }

    return image; // If fail/error on sws_scale or avpicture_alloc, it will simply be a null image
}
