#ifndef QOI_FORMAT_CODEC_QOI_H_
#define QOI_FORMAT_CODEC_QOI_H_

#include "utils.h"

constexpr uint8_t QOI_OP_INDEX_TAG = 0x00;
constexpr uint8_t QOI_OP_DIFF_TAG  = 0x40;
constexpr uint8_t QOI_OP_LUMA_TAG  = 0x80;
constexpr uint8_t QOI_OP_RUN_TAG   = 0xc0; 
constexpr uint8_t QOI_OP_RGB_TAG   = 0xfe;
constexpr uint8_t QOI_OP_RGBA_TAG  = 0xff;
constexpr uint8_t QOI_PADDING[8] = {0u, 0u, 0u, 0u, 0u, 0u, 0u, 1u};
constexpr uint8_t QOI_MASK_2 = 0xc0;

/**
 * @brief encode the raw pixel data of an image to qoi format.
 *
 * @param[in] width image width in pixels
 * @param[in] height image height in pixels
 * @param[in] channels number of color channels, 3 = RGB, 4 = RGBA
 * @param[in] colorspace image color space, 0 = sRGB with linear alpha, 1 = all channels linear
 *
 * @return bool true if it is a valid qoi format image, false otherwise
 */
bool QoiEncode(uint32_t width, uint32_t height, uint8_t channels, uint8_t colorspace = 0);

/**
 * @brief decode the qoi format of an image to raw pixel data
 *
 * @param[out] width image width in pixels
 * @param[out] height image height in pixels
 * @param[out] channels number of color channels, 3 = RGB, 4 = RGBA
 * @param[out] colorspace image color space, 0 = sRGB with linear alpha, 1 = all channels linear
 *
 * @return bool true if it is a valid qoi format image, false otherwise
 */
bool QoiDecode(uint32_t &width, uint32_t &height, uint8_t &channels, uint8_t &colorspace);


bool QoiEncode(uint32_t width, uint32_t height, uint8_t channels, uint8_t colorspace) {
    QoiWriteChar('q');
    QoiWriteChar('o');
    QoiWriteChar('i');
    QoiWriteChar('f');
    QoiWriteU32(width);
    QoiWriteU32(height);
    QoiWriteU8(channels);
    QoiWriteU8(colorspace);

    uint8_t history[64][4] = {0};
    uint8_t pre_r = 0, pre_g = 0, pre_b = 0, pre_a = 255;
    int run = 0;

    auto write_pixel = [&](uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
        int index = QoiColorHash(r, g, b, a);
        if (history[index][0] == r && history[index][1] == g && history[index][2] == b && history[index][3] == a) {
            QoiWriteU8(QOI_OP_INDEX_TAG);
            QoiWriteU8(index);
        } else {
            uint8_t dr = r - pre_r;
            uint8_t dg = g - pre_g;
            uint8_t db = b - pre_b;
            uint8_t da = a - pre_a;

            if ((dr == 0 || dg == 0 || db == 0) && (channels == 3 || da == 0)) {
                QoiWriteU8(QOI_OP_DIFF_TAG);
                QoiWriteU8(dr); QoiWriteU8(dg); QoiWriteU8(db);
                if (channels == 4) QoiWriteU8(da);
            } else {
                uint8_t luma = (uint8_t)((r * 2 + g * 1 + b * 1) / 4);
                int dr_l = r - luma;
                int dg_l = g - luma;
                int db_l = b - luma;
                if (dr_l >= -64 && dr_l <= 63 && dg_l >= -64 && dg_l <= 63 && db_l >= -64 && db_l <= 63) {
                    QoiWriteU8(QOI_OP_LUMA_TAG);
                    QoiWriteU8(luma);
                    QoiWriteU8((uint8_t)(dr_l + 64));
                    QoiWriteU8((uint8_t)(dg_l + 64));
                    QoiWriteU8((uint8_t)(db_l + 64));
                } else {
                    QoiWriteU8(channels == 3 ? QOI_OP_RGB_TAG : QOI_OP_RGBA_TAG);
                    QoiWriteU8(r); QoiWriteU8(g); QoiWriteU8(b);
                    if (channels == 4) QoiWriteU8(a);
                }
            }
        }
        history[index][0] = r;
        history[index][1] = g;
        history[index][2] = b;
        history[index][3] = a;
        pre_r = r; pre_g = g; pre_b = b; pre_a = a;
    };

    uint64_t px_num = (uint64_t)width * height;
    for (uint64_t i = 0; i < px_num; ++i) {
        uint8_t r = QoiReadU8();
        uint8_t g = QoiReadU8();
        uint8_t b = QoiReadU8();
        uint8_t a = (channels == 4) ? QoiReadU8() : 255;

        if (r == pre_r && g == pre_g && b == pre_b && a == pre_a) {
            run++;
            if (run == 256) {
                QoiWriteU8(QOI_OP_RUN_TAG);
                QoiWriteU8(255);
                run = 0;
            }
        } else {
            if (run > 0) {
                QoiWriteU8(QOI_OP_RUN_TAG);
                QoiWriteU8(run - 1);
                run = 0;
            }
            write_pixel(r, g, b, a);
        }
    }
    if (run > 0) {
        QoiWriteU8(QOI_OP_RUN_TAG);
        QoiWriteU8(run - 1);
    }

    for (int i = 0; i < 8; ++i) QoiWriteU8(QOI_PADDING[i]);
    return true;
}

bool QoiDecode(uint32_t &width, uint32_t &height, uint8_t &channels, uint8_t &colorspace) {
    char magic[4];
    for (int i = 0; i < 4; ++i) magic[i] = QoiReadChar();
    if (magic[0] != 'q' || magic[1] != 'o' || magic[2] != 'i' || magic[3] != 'f') return false;

    width = QoiReadU32();
    height = QoiReadU32();
    channels = QoiReadU8();
    colorspace = QoiReadU8();

    uint8_t history[64][4] = {0};
    uint8_t pre_r = 0, pre_g = 0, pre_b = 0, pre_a = 255;
    uint64_t px_num = (uint64_t)width * height;
    uint64_t pixels_decoded = 0;

    while (pixels_decoded < px_num) {
        uint8_t tag = QoiReadU8();
        uint8_t r, g, b, a;

        if ((tag & QOI_MASK_2) == QOI_OP_RUN_TAG) {
            uint8_t run = QoiReadU8();
            r = pre_r; g = pre_g; b = pre_b; a = pre_a;
            for (int i = 0; i < run + 1; ++i) {
                if (pixels_decoded >= px_num) return false;
                QoiWriteU8(r); QoiWriteU8(g); QoiWriteU8(b);
                if (channels == 4) QoiWriteU8(a);
                pixels_decoded++;
            }
        } else if ((tag & QOI_MASK_2) == QOI_OP_INDEX_TAG) {
            uint8_t index = QoiReadU8();
            r = history[index][0]; g = history[index][1]; b = history[index][2]; a = history[index][3];
            QoiWriteU8(r); QoiWriteU8(g); QoiWriteU8(b);
            if (channels == 4) QoiWriteU8(a);
            pixels_decoded++;
        } else if ((tag & QOI_MASK_2) == QOI_OP_DIFF_TAG) {
            uint8_t dr = QoiReadU8();
            uint8_t dg = QoiReadU8();
            uint8_t db = QoiReadU8();
            uint8_t da = (channels == 4) ? QoiReadU8() : 0;
            r = pre_r + dr; g = pre_g + dg; b = pre_b + db; a = pre_a + da;
            QoiWriteU8(r); QoiWriteU8(g); QoiWriteU8(b);
            if (channels == 4) QoiWriteU8(a);
            pixels_decoded++;
        } else if ((tag & QOI_MASK_2) == QOI_OP_LUMA_TAG) {
            uint8_t luma = QoiReadU8();
            uint8_t dr = QoiReadU8();
            uint8_t dg = QoiReadU8();
            uint8_t db = QoiReadU8();
            r = luma + (dr - 64); g = luma + (dg - 64); b = luma + (db - 64); a = pre_a;
            QoiWriteU8(r); QoiWriteU8(g); QoiWriteU8(b);
            if (channels == 4) QoiWriteU8(a);
            pixels_decoded++;
        } else if (tag == QOI_OP_RGB_TAG) {
            r = QoiReadU8(); g = QoiReadU8(); b = QoiReadU8(); a = pre_a;
            QoiWriteU8(r); QoiWriteU8(g); QoiWriteU8(b);
            if (channels == 4) QoiWriteU8(a);
            pixels_decoded++;
        } else if (tag == QOI_OP_RGBA_TAG) {
            r = QoiReadU8(); g = QoiReadU8(); b = QoiReadU8(); a = QoiReadU8();
            QoiWriteU8(r); QoiWriteU8(g); QoiWriteU8(b);
            if (channels == 4) QoiWriteU8(a);
            pixels_decoded++;
        } else {
            return false;
        }

        int index = QoiColorHash(r, g, b, a);
        history[index][0] = r; history[index][1] = g; history[index][2] = b; history[index][3] = a;
        pre_r = r; pre_g = g; pre_b = b; pre_a = a;
    }

    for (int i = 0; i < 8; ++i) {
        if (QoiReadU8() != QOI_PADDING[i]) return false;
    }
    return true;
}

#endif // QOI_FORMAT_CODEC_QOI_H_
