/*
 * HTMLRenderer.h
 *
 *  Created on: Mar 15, 2011
 *      Author: tian
 */

#ifndef HTMLRENDERER_H_
#define HTMLRENDERER_H_

#include <algorithm>
#include <fstream>
#include <unordered_map>
#include <map>
#include <vector>

#include <OutputDev.h>
#include <GfxState.h>
#include <CharTypes.h>
#include <Stream.h>
#include <Array.h>
#include <Dict.h>
#include <XRef.h>
#include <Catalog.h>
#include <Page.h>
#include <PDFDoc.h>
#include <goo/gtypes.h>
#include <Object.h>
#include <GfxFont.h>

#include "Param.h"
#include "util.h"

using namespace std;

class HTMLRenderer : public OutputDev
{
    public:
        HTMLRenderer(const Param * param);
        virtual ~HTMLRenderer();

        void process(PDFDoc * doc);

        //---- get info about output device

        // Does this device use upside-down coordinates?
        // (Upside-down means (0,0) is the top left corner of the page.)
        virtual GBool upsideDown() { return gFalse; }

        // Does this device use drawChar() or drawString()?
        virtual GBool useDrawChar() { return gFalse; }

        // Does this device use beginType3Char/endType3Char?  Otherwise,
        // text in Type 3 fonts will be drawn with drawChar/drawString.
        virtual GBool interpretType3Chars() { return gFalse; }

        // Does this device need non-text content?
        virtual GBool needNonText() { return gFalse; }

        //----- initialization and control
        virtual GBool checkPageSlice(Page *page, double hDPI, double vDPI,
                int rotate, GBool useMediaBox, GBool crop,
                int sliceX, int sliceY, int sliceW, int sliceH,
                GBool printing, Catalog * catalogA,
                GBool (* abortCheckCbk)(void *data) = NULL,
                void * abortCheckCbkData = NULL)
        {
            docPage = page;
            catalog = catalogA;
            return gTrue;
        }


        // Start a page.
        virtual void startPage(int pageNum, GfxState *state);

        // End a page.
        virtual void endPage();

        //----- update state
        /*
         * To optmize false alarms
         * We just mark as changed, and recheck if they have been changed when we are about to output a new string
         */
        virtual void updateAll(GfxState * state);
        virtual void updateFont(GfxState * state);
        virtual void updateTextMat(GfxState * state);
        virtual void updateCTM(GfxState * state, double m11, double m12, double m21, double m22, double m31, double m32);
        virtual void updateTextPos(GfxState * state);
        virtual void updateTextShift(GfxState * state, double shift);
        virtual void updateFillColor(GfxState * state);

        //----- text drawing
        virtual void drawString(GfxState * state, GooString * s);

    private:
        void close_cur_line();

        // return the mapped font name
        long long install_font(GfxFont * font);

        static void output_to_file(void * outf, const char * data, int len);

        std::string dump_embedded_type1_font (Ref * id, GfxFont * font, long long fn_id);
        std::string dump_embedded_type1c_font (GfxFont * font, long long fn_id);
        std::string dump_embedded_opentypet1c_font (GfxFont * font, long long fn_id);
        std::string dump_embedded_truetype_font (GfxFont * font, long long fn_id);

        void install_embedded_font(GfxFont * font, const std::string & suffix, long long fn_id);
        void install_base_font(GfxFont * font, GfxFontLoc * font_loc, long long fn_id);
        void install_external_font (GfxFont * font, long long fn_id);

        long long install_font_size(double font_size);
        long long install_whitespace(double ws_width, double & actual_width);
        long long install_transform_matrix(const double * tm);
        long long install_color(const GfxRGB * rgb);

        /*
         * remote font: to be retrieved from the web server
         * local font: to be substituted with a local (client side) font
         */
        void export_remote_font(long long fn_id, const string & suffix, const string & format, GfxFont * font);
        void export_remote_default_font(long long fn_id);
        void export_local_font(long long fn_id, GfxFont * font, const string & original_font_name, const string & cssfont);
        std::string general_font_family(GfxFont * font);

        void export_font_size(long long fs_id, double font_size);
        void export_whitespace(long long ws_id, double ws_width);
        void export_transform_matrix(long long tm_id, const double * tm);
        void export_color(long long color_id, const GfxRGB * rgb);

        XRef * xref;
        Catalog *catalog;
        Page *docPage;

        // page info
        int pageNum ;
        double pageWidth ;
        double pageHeight ;



        // state tracking when processing pdf
        void check_state_change(GfxState * state);
        void reset_state_track();

        bool all_changed;

        // if we have a pending opened line
        bool line_opened;

        // current position
        double cur_tx, cur_ty; // real text position, in text coords
        bool text_pos_changed; 

        long long cur_fn_id;
        double cur_font_size;
        long long cur_fs_id; 
        bool font_changed;

        long long cur_tm_id;
        bool ctm_changed;
        bool text_mat_changed;
        
        // this is CTM * TextMAT in PDF, not only CTM
        // [4] and [5] are ignored, we'll calculate the position of the origin separately
        double cur_ctm[6]; // unscaled

        long long cur_color_id;
        GfxRGB cur_color;
        bool color_changed;

        // optmize for web
        // we try to render the final font size directly
        // to reduce the effect of ctm as much as possible
        
        // draw_ctm is cur_ctm scaled by 1/draw_scale, 
        // so everything redenered should be multiplied by draw_scale
        double draw_ctm[6];
        double draw_font_size;
        double draw_scale; 

        // the position of next char, in text coords
        double draw_tx, draw_ty; 

        ofstream html_fout, allcss_fout, fontscript_fout;

        class FontInfo{
            public:
                long long fn_id;
        };
        unordered_map<long long, FontInfo> font_name_map;
        map<double, long long> font_size_map;
        map<double, long long> whitespace_map;

        // transform matrix
        class TM{
            public:
                TM() {}
                TM(const double * m) {memcpy(_, m, sizeof(_));}
                bool operator < (const TM & m) const {
                    for(int i = 0; i < 6; ++i)
                    {
                        if(_[i] < m._[i] - EPS)
                            return true;
                        if(_[i] > m._[i] + EPS)
                            return false;
                    }
                    return false;
                }
                bool operator == (const TM & m) const {
                    return _tm_equal(_, m._);
                }
                double _[6];
        };

        map<TM, long long> transform_matrix_map;

        class Color{
            public:
                Color() {}
                Color(const GfxRGB * rgb) { 
                    _[0] = rgb->r;
                    _[1] = rgb->g;
                    _[2] = rgb->b;
                }
                bool operator < (const Color & c) const {
                    for(int i = 0; i < 3; ++i)
                    {
                        if(_[i] < c._[i])
                            return true;
                        if(_[i] > c._[i])
                            return false;
                    }
                    return false;
                }
                bool operator == (const Color & c) const {
                    for(int i = 0; i < 3; ++i)
                        if(_[i] != c._[i])
                            return false;
                    return true;
                }

                int _[3];
        };
        map<Color, long long> color_map; 

        const Param * param;
};

#endif /* HTMLRENDERER_H_ */
