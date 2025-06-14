#include "video.h"

#define FAIL_ON_FRAME_DECODE_NB_FAIL 10

#define ESTIMATE_DURATION_FROM_FRAME_NB 5
#define ESTIMATE_DURATION_FROM_FRAME_RESCALE 0.8

//#define DEBUG_VIDEO_READING

Prefs Video::_prefs;
int Video::_jpegQuality = _okJpegQuality;

Video::Video(const Prefs &prefsParam, const QString &filePathName) :
    _filePathName(filePathName)
{
    _prefs = prefsParam;
    //if(_prefs._numberOfVideos > _hugeAmountVideos)       //save memory to avoid crash due to 32 bit limit
    //   _jpegQuality = _lowJpegQuality;
}

Video::ProcessingResult Video::process()
{
    ProcessingResult result;
    result.success = false;
    result.video = this;

    auto err = this->internalProcess();
    if(!err.isEmpty()){
        result.errorMsg = err;
    }
    else{
        result.success = true;
    }
    return result;
}

QString Video::internalProcess(){
    if(this->_prefs.isVerbose())
        Message::Get()->add(QString("[%1] STARTING %2").arg(QTime::currentTime().toString(), this->_filePathName));

    if(!QFileInfo::exists(_filePathName))
    {
        return "file doesn't seem to exist";
    }
#ifdef Q_OS_MACOS
    else if(_filePathName.contains(".photoslibrary")){
        const QString fileNameNoExt = QFileInfo(_filePathName).completeBaseName();
        if(!_filePathName.contains(".photoslibrary/originals/")){
            return "file is an Apple Photos derivative";
        }
        else if(fileNameNoExt.contains("_")){
            return "video seems to be a live photo, so deal with it as a photo.";
        }
    }
#endif

    // THEODEBUG : probably should re-implement things not to cache randomly !
    Db cache(_prefs.cacheFilePathName()); // we open the db here, but we'll only store things if needed
    if(this->_prefs.useCacheOption()!=Prefs::NO_CACHE && cache.readMetadata(*this)) {      //check first if video properties are cached
        modified = QFileInfo(_filePathName).lastModified(); // Db doesn't cache the modified date
        if(QFileInfo(_filePathName).birthTime().isValid())
            _fileCreateDate = QFileInfo(_filePathName).birthTime();
    }
    else if(this->_prefs.useCacheOption()!=Prefs::CACHE_ONLY) {
        if(QFileInfo(_filePathName).size()==0){ // check this before, as it's faster, but getMetadata also does this but stores the info
            return "file size = 0 ";
        }
        auto err = getMetadata(_filePathName);
        if(!err.isEmpty()){        //as not cached, read metadata with ffmpeg
            return QString("could not read metadata: %1").arg(err);
        }
    }
    else{
        return "video was not fully cached ";
    }
    if(this->_prefs.useCacheOption()==Prefs::WITH_CACHE) // TODO-REFACTOR could we move this into the case when we actually cache data ?
        cache.writeMetadata(*this); // cache so next run will be faster
    bool durationWasZero = false; // we'll update cache again later if duration was 0
    if(duration==0)
        durationWasZero = true;

    if(width == 0 || height == 0)// || duration == 0) // no duration check as we can infer duration when decoding frames,
    {
        return QString("height (%1) or width (%2) = 0 ").arg(height).arg(width);
    }

    auto err = takeScreenCaptures(cache);
    if(this->_prefs.useCacheOption()==Prefs::WITH_CACHE && durationWasZero && duration!=0)
        cache.writeMetadata(*this); // update cache as takeScreenCaptures can estimate duration, when it was 0
    if(!err.isEmpty()){
        return QString("capture failed: %1").arg(err);
    }
    else if((this->_prefs.thumbnailsMode() != cutEnds && hash[0] == 0 ) || // only cutends separates hashes for captures, other just treat as one big capture
             (this->_prefs.thumbnailsMode() == cutEnds && hash[0] == 0 && hash[1] == 0)){   //all screen captures black
        return "all screen captures black";
    }
    else{
        return "";
    }
}

const QString Video::getMetadata(const QString &filename)
{
    const QFileInfo videoFile(filename);
    size = videoFile.size();
    if(size==0){
        return "video file size=0";
    }

    // Get Video stream metadata with new methods using ffmpeg library
    ffmpeg::av_log_set_level(AV_LOG_FATAL);
#ifdef DEBUG_VIDEO_READING
    ffmpeg::av_log_set_level(AV_LOG_INFO);
#endif
    ffmpeg::AVFormatContext *fmt_ctx = NULL;
    int ret;
    ret = ffmpeg::avformat_open_input(&fmt_ctx, QDir::toNativeSeparators(filename).toStdString().c_str(), NULL, NULL);
    // TODO: get error description with ffmpeg::av_strerror
    if (ret < 0) {
        avformat_close_input(&fmt_ctx);
        return "could not open input, file is probably broken";
    }
    ret = ffmpeg::avformat_find_stream_info(fmt_ctx, NULL);
    if (ret < 0) {
        avformat_close_input(&fmt_ctx);
        return "could not find stream information";
    }
#ifdef DEBUG_VIDEO_READING
    // Helpful for debugging : dumps as the executable would
    qDebug() << "ffmpeg dump format :";
    ffmpeg::av_dump_format(fmt_ctx, 0, QDir::toNativeSeparators(filename).toStdString().c_str(), 0);
    qDebug() << "\n\n";
#endif
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
    if(ret<0){ // Didn't find a video stream
        ffmpeg::avformat_close_input(&fmt_ctx);
        return "could not find a video stream, only audio or others";
    }
    ffmpeg::AVStream *vs = fmt_ctx->streams[ret]; // not necessary, but shorter for next calls
    codec = ffmpeg::avcodec_get_name(vs->codecpar->codec_id); //inspired from avcodec_string function
    // sometimes duration and or bitrate from format context is not found, so we set it here from video stream
    if(duration==0 && vs->duration!=AV_NOPTS_VALUE){
        // duration is in ms, so we *1000 to go from secs to ms. Also video stream duration is in its own time base
        duration = 1000*vs->duration*vs->time_base.num/vs->time_base.den;
    }
    if(bitrate==0 && vs->codecpar->bit_rate!=AV_NOPTS_VALUE){
        bitrate = vs->codecpar->bit_rate/1000; //we want it in kb/s and ffmpeg gives b/s
    }
    width = vs->codecpar->width ; // from avcodec_parameters_to_context function -> different than vs->codec->coded_height but seems to be more correct
    height = vs->codecpar->height;
    if(vs->avg_frame_rate.den!=0){
        framerate = round(ffmpeg::av_q2d(vs->avg_frame_rate) * 10) / 10;
    }

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
    }

    // Get GPS and other video metadata
    const ffmpeg::AVDictionaryEntry *entry = NULL;
    while ((entry = ffmpeg::av_dict_iterate(fmt_ctx->metadata, entry))) {
        auto key = QString::fromUtf8(entry->key);
        auto value = QString::fromUtf8(entry->value);
        this->meta.additionalMetadata.insert(key, value);
        if(this->meta.gpsCoordinates.isEmpty()
           && key == "location") {
            this->meta.gpsCoordinates = value;
        }
    }

    // Find audio stream information (we don't care if we don't find any, though)
    ret = ffmpeg::av_find_best_stream(fmt_ctx,ffmpeg::AVMEDIA_TYPE_AUDIO, -1 /* auto stream selection*/,
                                      -1 /* no related stream finding*/, NULL /*no decoder return*/, 0 /* no flags*/);
    if(ret>=0){ // Found a good audio stream
        ffmpeg::AVStream *as = fmt_ctx->streams[ret]; // not necessary, but shorter for next calls

        const QString audioCodec = ffmpeg::avcodec_get_name(as->codecpar->codec_id);// from avcodec_string function
        const QString rate = QString::number(as->codecpar->sample_rate);
        // FFmpeg 7.x: Use AVChannelLayout and av_channel_layout_describe
        ffmpeg::AVCodecContext *audio_ctx = ffmpeg::avcodec_alloc_context3(NULL);
        if (audio_ctx && ffmpeg::avcodec_parameters_to_context(audio_ctx, as->codecpar) >= 0) {
            char buf[128] = {0};
            ffmpeg::av_channel_layout_describe(&audio_ctx->ch_layout, buf, sizeof(buf));
            QString channels = QString::fromUtf8(buf);
            if (audio_ctx->ch_layout.nb_channels == 1)
                channels = QStringLiteral("mono");
            else if (audio_ctx->ch_layout.nb_channels == 2)
                channels = QStringLiteral("stereo");
            audio = QStringLiteral("%1 %2 Hz %3").arg(audioCodec, rate, channels);

            const int bits_per_sample = ffmpeg::av_get_bits_per_sample(as->codecpar->codec_id);
            const int bitrate = bits_per_sample ? as->codecpar->sample_rate * (int64_t)audio_ctx->ch_layout.nb_channels * bits_per_sample/1000 : as->codecpar->bit_rate/1000;
            const QString kbps = QString::number(bitrate);
            if(!kbps.isEmpty() && kbps != QStringLiteral("0"))
                audio = QStringLiteral("%1 %2 kb/s").arg(audio, kbps);
        }
        if (audio_ctx) ffmpeg::avcodec_free_context(&audio_ctx);
    }

    ffmpeg::avformat_close_input(&fmt_ctx);

    modified = videoFile.lastModified(); // get it at the end so as not to have a date when other infos are empty
    if(videoFile.birthTime().isValid())
        _fileCreateDate = videoFile.birthTime();

    return ""; // success !
}

// returns empty string if success or string with error message
const QString Video::takeScreenCaptures(const Db &cache)
{
    Thumbnail thumb(this->_prefs.thumbnailsMode());
    QImage thumbnail(thumb.cols() * width, thumb.rows() * height, QImage::Format_RGB888);
    const QVector<int> percentages = thumb.percentages(); // percent from 1 to 100
    int capture = percentages.count();
    int ofDuration = 100; // used to "rescale" total duration... as duration*percent*ofDuration

    // TODO: investigate if sometimes takes very long time or just cap at x number of tried captures or something not only %
    while(--capture >= 0)           //screen captures are taken in reverse order so errors are found early
    {
        QMutexLocker locker(&this->progressLock);
        this->progress++;
        locker.unlock();

        QImage frame;
        QByteArray cachedImage;
        if(this->_prefs.useCacheOption()!=Prefs::NO_CACHE) // TODO-REFACTOR could maybe load from cache in same condition as frame loading and resizing... ?
            cachedImage = cache.readCapture(_filePathName, percentages[capture]);
        QBuffer captureBuffer(&cachedImage);
        bool writeToCache = false;

        if(this->_prefs.useCacheOption()!=Prefs::NO_CACHE && !cachedImage.isNull())   //image was already in cache
        {
            frame.load(&captureBuffer, QByteArrayLiteral("JPG"));   //was saved in cache as small size, resize to original
            frame = frame.scaled(width, height, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        }
        else if (this->_prefs.useCacheOption()!=Prefs::CACHE_ONLY){
            frame = ffmpegLib_captureAt(percentages[capture], ofDuration);
            if(!frame.isNull() && this->_prefs.useCacheOption()==Prefs::WITH_CACHE)
                writeToCache = true;
        }

        if(frame.isNull())                                  //taking screen capture may fail if video is broken
        {
            ofDuration = ofDuration - _goBackwardsPercent;
            if(ofDuration >= _videoStillUsable)             //retry a few times, always closer to beginning
            {
                capture = percentages.count();
                continue;
            }
            QString err;
            if(this->_prefs.useCacheOption()!=Prefs::CACHE_ONLY)
                err = QString("not enough useable video at %1%, even after total duration rescaled to %2%").arg(percentages[capture]).arg(ofDuration);
            else
                err = QString("cache only mode but capture was not cached at %1%, even after total duration rescaled to %2%").arg(percentages[capture]).arg(ofDuration);
            return err;
        }

        if(frame.width() > width || frame.height() > height){    //metadata parsing error or variable resolution
            return QString("capture height=%1 width=%2 is different from metadata height=%3 width=%4")
                .arg(frame.height())
                .arg(frame.width())
                .arg(height)
                .arg(width);
        }

        QPainter painter(&thumbnail);                           //copy captured frame into right place in thumbnail
        painter.drawImage(capture % thumb.cols() * width, capture / thumb.cols() * height, frame);

        if(writeToCache) {
            frame = minimizeImage(frame);
            frame.save(&captureBuffer, QByteArrayLiteral("JPG"), _okJpegQuality);
            cache.writeCapture(_filePathName, percentages[capture], cachedImage);
        }
    }

    const int hashes = this->_prefs.thumbnailsMode() == cutEnds? 2 : 1;    //if cutEnds mode: separate hash for beginning and end
    processThumbnail(thumbnail, hashes);
    return "";
}

void Video::processThumbnail(QImage &thumbnail, const int &hashes)
{
    for(int hash=0; hash<hashes; hash++)
    {
        QImage image = thumbnail;
        if(this->_prefs.thumbnailsMode() == cutEnds)           //if cutEnds mode: separate thumbnail into first and last frames
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

QImage Video::ffmpegLib_captureAt(const int percent, const int ofDuration)
{
    ffmpeg::av_log_set_level(AV_LOG_FATAL);
#ifdef DEBUG_VIDEO_READING
    // NB Here a lot of debug elements are kept as is, because they quickly enable diagnstics of new or problematic
    // video codecs/files/...
    qDebug() << "Library will try to capture at "<<percent<<"% ofduration "<<ofDuration<<"% for file "<< _filePathName;
    ffmpeg::av_log_set_level(AV_LOG_DEBUG);
#endif
    // new ffmpeg library code to capture an image :
    // inspired from http://blog.allenworkspace.net/2020/06/ffmpeg-decode-and-then-encode-frames-to.html
    // general understanding https://github.com/leandromoreira/ffmpeg-libav-tutorial
    // qImage from avframe : https://stackoverflow.com/questions/13088749/efficient-conversion-of-avframe-to-qimage
    QImage img;

    ffmpeg::AVFormatContext *fmt_ctx = NULL;

    /* open input file, and allocate format context */
    if (ffmpeg::avformat_open_input(&fmt_ctx, QDir::toNativeSeparators(_filePathName).toStdString().c_str(), NULL, NULL) < 0) {
        qDebug() << "Could not open source file " << _filePathName;
        avformat_close_input(&fmt_ctx);
        return img;
    }

    /* retrieve stream information */
    if (ffmpeg::avformat_find_stream_info(fmt_ctx, NULL) < 0) {
        qDebug() << "Could not find stream information" << _filePathName;
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
        qDebug() << "Could not find a good video stream" << _filePathName;
        ffmpeg::avformat_close_input(&fmt_ctx);
        return img;
    }
    ffmpeg::AVStream *vs = fmt_ctx->streams[stream_index]; // set shorter reference to video stream of interest
    // av_find_best_stream should not return non video stream if video stream requested !
    if(vs->codecpar->codec_type!=ffmpeg::AVMEDIA_TYPE_VIDEO){
        qDebug() << "FFMPEG returned stream was not video... !" << _filePathName;
        ffmpeg::avformat_close_input(&fmt_ctx);
        return img;
    }

    // find decoder for the stream
    const ffmpeg::AVCodec *codec = ffmpeg::avcodec_find_decoder(vs->codecpar->codec_id); // null if none found
    if (!codec) {
        qDebug() << "Failed to find video codec "<< ffmpeg::avcodec_get_name(vs->codecpar->codec_id) << " for file " << _filePathName;
        ffmpeg::avformat_close_input(&fmt_ctx);
        return img;
    }

    /* Allocate a codec context for the decoder */
    ffmpeg::AVCodecContext *codec_ctx = ffmpeg::avcodec_alloc_context3(codec);
    if (!codec_ctx) {
        qDebug() << "Failed to allocate the video codec context for file " << _filePathName;
        // NB here codec context failed to alloc, but next steps will need to free it !
        ffmpeg::avformat_close_input(&fmt_ctx);
        return img;
    }

    /* Copy codec parameters from input stream to codec context */
    if (ffmpeg::avcodec_parameters_to_context(codec_ctx, vs->codecpar) < 0) {
        qDebug() << "Failed to copy codec parameters to decoder context for file" << _filePathName;
        ffmpeg::avcodec_free_context(&codec_ctx);
        ffmpeg::avformat_close_input(&fmt_ctx);
        return img;
    }

    /* Open the codec */
    if(ffmpeg::avcodec_open2(codec_ctx, codec, NULL /* no options*/)<0){
        qDebug() << "Failed to open video codec for file " << _filePathName;
        ffmpeg::avcodec_free_context(&codec_ctx);
        ffmpeg::avformat_close_input(&fmt_ctx);
        return img;
    }

    /* Allocate packet for storing decoded data */
    ffmpeg::AVPacket* vPacket = ffmpeg::av_packet_alloc();
    if (!vPacket){
        qDebug() << "failed to allocated memory for AVPacket for file " << _filePathName;
        //ffmpeg::av_packet_free(&vPacket); //NB failed to alloc, so don't call yet but next time
        ffmpeg::avcodec_free_context(&codec_ctx);
        ffmpeg::avformat_close_input(&fmt_ctx);
        return img;
    }

    /* Allocate a frame */
    ffmpeg::AVFrame* vFrame = ffmpeg::av_frame_alloc();
    if (!vFrame){
        qDebug() << "failed to allocated memory for frame for file " << _filePathName;
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
    int64_t ms_stream_duration = 0;
    if(vs->duration!=AV_NOPTS_VALUE && vs->time_base.den!=0){
        ms_stream_duration = 1000*vs->duration*vs->time_base.num/vs->time_base.den;
    }
    else if(duration!=0){
        ms_stream_duration = duration;
    }
    int frames_estimated=-1; // duration estimation from frame size (bytes) and file size. Disabled if -1
    int avg_frame_bytes_size = 1; // avoid 0 division
    if(ms_stream_duration==0){
        frames_estimated = 0;
    }
    // TODO : we use stream duration here, not duration gotten from metadata,
    //        but sometimes metadata file duration is very different => should find a way to report it
#ifdef DEBUG_VIDEO_READING
    if(abs(ms_stream_duration-duration)>100){
        qDebug() << "Stream duration ms(" << ms_stream_duration << ") is more than 100ms different to file reported duration("<<duration<<") (we're using video stream duration for captures) " << _filePathName;
    }
#endif
    int64_t wanted_ms_place = (double) ms_stream_duration *  percent * ofDuration / (100 * 100 /*percents*/) ;
    int64_t ts_diff = 0;
    if(vs->avg_frame_rate.num!=0 && vs->time_base.num!=0){// avg framerate can be 0/0 -> add 1 and ts_diff can be 0
        ts_diff = vs->avg_frame_rate.den * vs->time_base.den / (vs->avg_frame_rate.num * vs->time_base.num);
    }
#ifdef DEBUG_VIDEO_READING
    long long start_time=0;
    if(vs->start_time!=AV_NOPTS_VALUE){
        start_time = vs->start_time;
    }
#endif
    // don't need to offset by vs->start_time as stream duration doesn't seem to include this offset
    int64_t wanted_ts = 0;
    if(vs->time_base.num!=0){
        wanted_ts = vs->time_base.den * wanted_ms_place / (vs->time_base.num * 1000 /*we have ms duration*/);
    }
#ifdef DEBUG_VIDEO_READING
    qDebug() << "wanted_ms_place "<< wanted_ms_place << " for video stream ms duration "<< ms_stream_duration;
    qDebug() << "ts_diff_between_images "<< ts_diff << " in terms of time "<< msToHHMMSS(1000*ts_diff*vs->time_base.num/vs->time_base.den);
    qDebug() << "time base = "<<vs->time_base.num<<" / "<<vs->time_base.den;
    qDebug() << "avg fps="<<(float)vs->avg_frame_rate.num/vs->avg_frame_rate.den << " stream bitrate= "<<vs->codecpar->bit_rate/1000<<"kb/s"<< "Number of frames in stream : "<<vs->nb_frames;
    qDebug() << "start pts="<< start_time << "in terms of time "<< msToHHMMSS(1000*start_time*vs->time_base.num/vs->time_base.den);
    qDebug() << "stream duration="<< vs->duration << " in terms of time " << msToHHMMSS(ms_stream_duration) << " in terms of ms "<< ms_stream_duration;
    qDebug() << "Seeking to timestamp " <<
        msToHHMMSS(ms_stream_duration * (percent * ofDuration) / (100 * 100))<<
        " for file (of duration " << msToHHMMSS(ms_stream_duration) << " ) ";
    qDebug() << "(in terms of stream time base wanted : "<< wanted_ts << " for stream duration " << vs->duration<<")";
#endif
    // From http://ffmpeg.org/ffmpeg.html#Main-options see -ss option (as was previously used with executable)
    // "ffmpeg will seek to the closest seek point before position"
    // -> need to seek to closest previous keyframe and then decode each frame until the one right BEFORE (or at/after) exact position
    // in fact from manual tests with the executable, it seems the behavior was not very consistent. Will just seek to frame right before or after timestamp
    if(ffmpeg::av_seek_frame(fmt_ctx, stream_index, wanted_ts, AVSEEK_FLAG_BACKWARD /* 0 for no flags*/) < 0){ // will seek to closest previous keyframe
        qDebug() << "failed to seek to frame at timestamp " << msToHHMMSS(1000*wanted_ts*vs->time_base.num/vs->time_base.den) << " for file " << _filePathName;
        ffmpeg::av_packet_free(&vPacket);
        ffmpeg::av_frame_free(&vFrame);
        ffmpeg::avcodec_free_context(&codec_ctx);
        ffmpeg::avformat_close_input(&fmt_ctx);
        return img;
    }
    while (readFrame == false){
        QMutexLocker locker(&this->progressLock);
        this->progress++;
        locker.unlock();

        int ret = ffmpeg::av_read_frame(fmt_ctx, vPacket); // reads packet frames, but buffers some so at the end,
            // even if it says EOF we need to decode again until there's no frames returned
#ifdef DEBUG_VIDEO_READING
        if(ret == AVERROR_EOF){
            qDebug() << "av_read_frame said eof for file " << _filePathName;
        }
#endif
        if( ret /* reads all stream frames, must check for when video*/ < 0){
            failedFrames++;
#ifdef DEBUG_VIDEO_READING
            qDebug() << "Failed to read 1 frame, will try again for file" << _filePathName;
#endif
            if(failedFrames>FAIL_ON_FRAME_DECODE_NB_FAIL){
                qDebug() << "Failed to read frames too many("<< failedFrames << ") times, will stop, for file " << _filePathName;
                ffmpeg::av_packet_free(&vPacket);
                ffmpeg::av_frame_free(&vFrame);
                ffmpeg::avcodec_free_context(&codec_ctx);
                ffmpeg::avformat_close_input(&fmt_ctx);
                return img;
            }
            if(ret != AVERROR_EOF){
                // TODO ? unref packet ?
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
                    qDebug() << "Reached end of file but no matching frame for " << _filePathName;
                }
                else{
                    qDebug() << "Failed to send raw packet to decoder for file " << _filePathName;
                }
                ffmpeg::av_packet_free(&vPacket);
                ffmpeg::av_frame_free(&vFrame);
                ffmpeg::avcodec_free_context(&codec_ctx);
                ffmpeg::avformat_close_input(&fmt_ctx);
                return img;
            }
            if(ffmpeg::avcodec_receive_frame(codec_ctx, vFrame) == 0){ // should debug codes, 0 means success, otherwise just try again
                long long curr_ts = -1;
                if(vFrame->best_effort_timestamp!=AV_NOPTS_VALUE){
                    curr_ts = vFrame->best_effort_timestamp;
                }
                else if(vFrame->pts!=AV_NOPTS_VALUE){
                    curr_ts = vFrame->pts;
                }
                else if(vPacket->pts!=AV_NOPTS_VALUE){
                    curr_ts = vPacket->pts;
                }
                else if(vFrame->pkt_dts!=AV_NOPTS_VALUE){
                    curr_ts = vFrame->pkt_dts;
                }
#ifdef DEBUG_VIDEO_READING
                //                saveToJPEG(codec_ctx, vFrame, imgPathname +  " pts" + msToHHMMSS((long long)1000*vFrame->pts*vs->time_base.num/vs->time_base.den) + ".bmp");
                qDebug() << "Frame " <<
                    "type "<< av_get_picture_type_char(vFrame->pict_type) <<
                    " frame number " << codec_ctx->frame_number;
                qDebug() << " wanted ts (from start)"<<wanted_ts<<" in time (from 0)"<<msToHHMMSS(1000*(wanted_ts-start_time)*vs->time_base.num/vs->time_base.den)<<
                    " vFrame pts "<<vFrame->pts << " in time (from0)"<< msToHHMMSS(1000*(vFrame->pts-start_time)*vs->time_base.num/vs->time_base.den) <<
                    " pkt dts " << vFrame->pkt_dts<<
                    " curr_ts "<<curr_ts <<
                    " best effort ts "<<vFrame->best_effort_timestamp;
#endif
    // try to find estimate of frame rate TODO : find another way to do this, with frame ts maybe... ?
                if(this->framerate==0){
                    ffmpeg::AVRational est_fps = ffmpeg::av_guess_frame_rate(fmt_ctx, vs,vFrame);
                    if(est_fps.num!=0 && est_fps.den!=0){
                        this->framerate = round(ffmpeg::av_q2d(est_fps) * 10) / 10;
                    }
                }
                // try to find estimate stream duration from frame size and file size
                if(frames_estimated!=-1 && frames_estimated<ESTIMATE_DURATION_FROM_FRAME_NB && vFrame->pkt_size!=0
                    && vs->time_base.num!=0 /*avoid 0 divisions*/){
                    avg_frame_bytes_size = (frames_estimated*avg_frame_bytes_size + vFrame->pkt_size)/(1+frames_estimated);
                    frames_estimated++;
                    ms_stream_duration = (double)ESTIMATE_DURATION_FROM_FRAME_RESCALE*1000*this->size*vs->avg_frame_rate.den/(avg_frame_bytes_size*vs->avg_frame_rate.num+1); // rescale to avoid over estimating, add 1 because avg_frame_rate can be 0 !
                    wanted_ms_place = (double) ms_stream_duration *  percent * ofDuration / (100 * 100 /*percents*/) ;
                    wanted_ts = vs->time_base.den * wanted_ms_place / (vs->time_base.num * 1000 /*we have ms duration*/);
                    this->duration = ms_stream_duration;
#ifdef DEBUG_VIDEO_READING
                    qDebug() <<"No video duration : trying to estimate from file size and frame size : "<<
                        " frame pkt size "<<vFrame->pkt_size<<
                        " avg "<< frames_estimated<<" frames size "<<avg_frame_bytes_size<<
                        " for file size (in bytes)"<<size <<
                        " making estimated nb frames "<<size/avg_frame_bytes_size<<
                        " or estimated duration " << msToHHMMSS(1000*vs->avg_frame_rate.den*size/(avg_frame_bytes_size*vs->avg_frame_rate.num));
#endif
                    if(frames_estimated==ESTIMATE_DURATION_FROM_FRAME_NB){
                        if(ffmpeg::av_seek_frame(fmt_ctx, stream_index, wanted_ts, AVSEEK_FLAG_BACKWARD /* 0 for no flags*/) < 0){ // will seek to closest previous keyframe
                            qDebug() << "failed to seek to frame at timestamp " << msToHHMMSS(1000*wanted_ts*vs->time_base.num/vs->time_base.den) << " for file " << _filePathName;
                            ffmpeg::av_packet_free(&vPacket);
                            ffmpeg::av_frame_free(&vFrame);
                            ffmpeg::avcodec_free_context(&codec_ctx);
                            ffmpeg::avformat_close_input(&fmt_ctx);
                            return img;
                        }
                    }
                }
                if((curr_ts-wanted_ts)>=-ts_diff){ // must read frames until we get before wanted time stamp
                    img = getQImageFromFrame(vFrame);
                    if(img.isNull()){
                        qDebug() << "Failed to save img for file " << _filePathName;
                        ffmpeg::av_packet_free(&vPacket);
                        ffmpeg::av_frame_free(&vFrame);
                        ffmpeg::avcodec_free_context(&codec_ctx);
                        ffmpeg::avformat_close_input(&fmt_ctx);
                        return QImage();
                    }
                    readFrame = true;
                }
            }
            else{
#ifdef DEBUG_VIDEO_READING
                qDebug() << "Failed to receive frame for "<< _filePathName;
#endif
            }
        }
        else{
#ifdef DEBUG_VIDEO_READING
            qDebug() << "Packet not of wanted stream for "<< _filePathName;
#endif
            if(ret == AVERROR_EOF){
                // TODO : could actually update stream duration because we know it more accurately now !!!
                qDebug() << "Reached end of file, no more stream packets, found no matching frame for " << _filePathName;
                ffmpeg::av_packet_free(&vPacket);
                ffmpeg::av_frame_free(&vFrame);
                ffmpeg::avcodec_free_context(&codec_ctx);
                ffmpeg::avformat_close_input(&fmt_ctx);
                return img;
            }
        }
        ffmpeg::av_packet_unref(vPacket);
        ffmpeg::av_frame_unref(vFrame);
    }

    ffmpeg::av_packet_free(&vPacket);
    ffmpeg::av_frame_free(&vFrame);
    ffmpeg::avcodec_free_context(&codec_ctx);
    ffmpeg::avformat_close_input(&fmt_ctx);

    if(img.isNull()){
        qDebug() << "ERROR with taking frame function, it is empty but shouldn't be !!! "<< _filePathName;
    }
#ifdef DEBUG_VIDEO_READING
    else{
        qDebug() << "Library capture success at timestamp " << msToHHMMSS(wanted_ms_place) << " for file " << _filePathName;
    }
#endif
    return img;
}

QImage Video::getQImageFromFrame(const ffmpeg::AVFrame* pFrame) const
{
    // first convert frame to rgb24
    ffmpeg::SwsContext* img_convert_ctx = ffmpeg::sws_getContext(
        pFrame->width,
        pFrame->height,
        (ffmpeg::AVPixelFormat)pFrame->format,
        pFrame->width,
        pFrame->height,
        ffmpeg::AV_PIX_FMT_RGB24,
        SWS_BICUBIC, NULL, NULL, NULL); // TODO : could we change to something else than bicubic ???
    if(!img_convert_ctx){
        qDebug() << "Failed to create sws context "<< _filePathName;
        return QImage();
    }

    // ---------------------------
    // Possibility 1 : convert to second frame, then load into QImage : has memory leak issue !!!
    //    ffmpeg::AVFrame* frameRGB = ffmpeg::av_frame_alloc();
    //    frameRGB->width = pFrame->width;
    //    frameRGB->height = pFrame->height;
    //    frameRGB->format = ffmpeg::AV_PIX_FMT_RGB24;
    //    ffmpeg::avpicture_alloc((ffmpeg::AVPicture*)frameRGB,
    //                            ffmpeg::AV_PIX_FMT_RGB24,
    //                            pFrame->width,
    //                            pFrame->height);

    //    if(ffmpeg::sws_scale(img_convert_ctx,
    //                pFrame->data,
    //                pFrame->linesize, 0,
    //                pFrame->height,
    //                frameRGB->data,
    //                frameRGB->linesize)
    //            != pFrame->height){
    //        qDebug() << "Error changing frame color range "<<filename;
    //        ffmpeg::av_frame_free(&frameRGB);
    //        ffmpeg::sws_freeContext(img_convert_ctx);
    //        return QImage();
    //    }

    //    QImage image(frameRGB->data[0],
    //                     pFrame->width,
    //                     pFrame->height,
    //                     frameRGB->linesize[0],
    //                     QImage::Format_RGB888);

    ////    ffmpeg::avpicture_free((ffmpeg::AVPicture*)frameRGB); // Problem as this frees the underlying data which QImage share
    //    ffmpeg::av_frame_free(&frameRGB);

    // ---------------------------
    // Possibility 2 : directly convert into QImage -> has problem when source frame width is not multiple of 4 (QImage ligns are 32 bit aligned)
    //    QImage image(pFrame->width,
    //                 pFrame->height,
    //                 QImage::Format_RGB888);

    //    int rgb_linesizes[8] = {0};
    //    rgb_linesizes[0] = 3*pFrame->width;

    //    if(ffmpeg::sws_scale(img_convert_ctx,
    //                pFrame->data,
    //                pFrame->linesize, 0,
    //                pFrame->height,
    //                (uint8_t *[]){image.bits()},
    //                rgb_linesizes)
    //            != pFrame->height){
    //        qDebug() << "Error changing frame color range "<<filename;
    //        ffmpeg::sws_freeContext(img_convert_ctx);
    //        return QImage();
    //    }

    // ---------------------------
    // Possibility 3 : modif attempt of 1 without deprecated stuff
    //    int rgb_linesizes[8] = {0};
    //    rgb_linesizes[0] = 3*pFrame->width;

    //    unsigned char* rgbData[8];
    //    int imgBytesSyze = 3*pFrame->height*pFrame->width;
    //    rgbData[0] = (unsigned char *)malloc(imgBytesSyze);
    //    if(ffmpeg::sws_scale(img_convert_ctx,
    //                pFrame->data,
    //                pFrame->linesize, 0,
    //                pFrame->height,
    //                rgbData,
    //                rgb_linesizes)
    //            != pFrame->height){
    //        qDebug() << "Error changing frame color range "<<filename;
    //        free(rgbData[0]);
    //        ffmpeg::sws_freeContext(img_convert_ctx);
    //        return QImage();
    //    }

    //    QImage image(rgbData[0],
    //                 pFrame->width,
    //                 pFrame->height,
    //                 rgb_linesizes[0],
    //                 QImage::Format_RGB888);

    // ---------------------------
    // Possibility 4 : modif attempt of 3 to do memcopy
    QImage image(pFrame->width,
                 pFrame->height,
                 QImage::Format_RGB888);

    int rgb_linesizes[8] = {0};
    rgb_linesizes[0] = 3*pFrame->width;

    unsigned char* rgbData[8];
    int imgBytesSyze = 3*pFrame->height*pFrame->width;
    rgbData[0] = (unsigned char *)malloc(imgBytesSyze+64); // ask for extra bytes just to be safe
    if(!rgbData[0]){
        qDebug() << "Error allocating buffer for frame conversion "<<_filePathName;
        free(rgbData[0]);
        ffmpeg::sws_freeContext(img_convert_ctx);
        return QImage();
    }
    if(ffmpeg::sws_scale(img_convert_ctx,
                          pFrame->data,
                          pFrame->linesize, 0,
                          pFrame->height,
                          rgbData, // ideally should be (uint8_t *[]){image.bits()} but not possible as explained below, QImage lines are at least 32 bit aligned
                          rgb_linesizes)
        != pFrame->height){
        qDebug() << "Error changing frame color range "<<_filePathName;
        free(rgbData[0]);
        ffmpeg::sws_freeContext(img_convert_ctx);
        return QImage();
    }

    // we need to copy line by line, as qimage data lines are 32 bit aligned, thus we will (need) leave empty space at the end
    // https://doc.qt.io/qt-5/qimage.html#scanLine -> here it mentions that each data line is minimum 32 bit aligned : this explains
    // why it's not possible to have sws_scale directly fill the QImage buffer, because it assumes each output line must
    // be filled entirely, I've not found a way that makes it "skip the rest of the line" but only output a correct width... !
    // So doing sws_scale with output data being QImage.bits() works for all source frames that are "by chance" 32 bit aligned, but
    // not others. Thus, we have another buffer, and copy afterwards to qimage, although this means sws_scale copies, and we copy so it's
    // two times the same operation...
    for(int y=0; y<pFrame->height; y++){
        memcpy(image.scanLine(y), rgbData[0]+y*3*pFrame->width, 3*pFrame->width);
    }

    free(rgbData[0]);
    ffmpeg::sws_freeContext(img_convert_ctx);

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

// ------------------------------------------------------------
// ------------ Start: Public STATIC member functions ----------
VideoMetadata Video::videoToMetadata(const Video & vid) {
    VideoMetadata meta;
    meta.filename = vid._filePathName;
    meta.nameInApplePhotos = vid.nameInApplePhotos;
    meta.size = vid.size;
    meta._fileCreateDate = vid._fileCreateDate;
    meta.modified = vid.modified;
    meta.duration = vid.duration;
    meta.bitrate = vid.bitrate;
    meta.framerate = vid.framerate;
    meta.codec = vid.codec;
    meta.audio = vid.audio;
    meta.width = vid.width;
    meta.height = vid.height;
    meta.gpsCoordinates = vid.meta.gpsCoordinates; // Added to include GPS data
    return meta;
}
// ------------ End: Public STATIC member functions ----------
// ------------------------------------------------------------
