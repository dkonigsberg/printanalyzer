#include "contrast.h"

#include <stddef.h>

const contrast_grade_t CONTRAST_WHOLE_GRADES[] = {
    CONTRAST_GRADE_00,
    CONTRAST_GRADE_0, CONTRAST_GRADE_1, CONTRAST_GRADE_2,
    CONTRAST_GRADE_3, CONTRAST_GRADE_4, CONTRAST_GRADE_5
};

static const char *CONTRAST_GRADE_STR[] = {
    "00", "0", "1/2", "1", "1-1/2", "2", "2-1/2", "3", "3-1/2",
    "4", "4-1/2", "5"
};

static const char *CONTRAST_FILTER_NAME_STR[] = {
    "Regular",
    "Durst (170M)", "Durst (130M)", "Kodak",
    "Focomat V35", "Meopta"
};

static const char *GRADE_FILTER_STR_DURST_170M[] = {
    "115Y+0M", "100Y+5M", "88Y+7M",   "75Y+10M",
    "65Y+15M", "52Y+20M", "42Y+28M",  "34Y+45M",
    "27Y+60M", "17Y+76M", "10Y+105M", "0Y+170M"
};

static const char *GRADE_FILTER_STR_DURST_130M[] = {
    "120Y+0M", "88Y+6M",  "78Y+8M",  "64Y+12M",
    "53Y+17M", "45Y+24M", "35Y+31M", "24Y+42M",
    "17Y+53M", "10Y+69M", "6Y+89M",  "0Y+130M"
};

static const char *GRADE_FILTER_STR_KODAK[] = {
    "162Y+0M", "90Y+0M",  "78Y+5M",  "68Y+10M",
    "49Y+23M", "41Y+32M", "32Y+42M", "23Y+56M",
    "15Y+75M", "6Y+102M", "0Y+150M", "0Y+200M"
};

static const char *GRADE_FILTER_STR_LEITZ_FOCOMAT_V35[] = {
    "135Y+6M", "105Y+12M", "77Y+11M",  "67Y+17M",
    "52Y+28M", "39Y+43M",  "32Y+51M",  "23Y+62M",
    "14Y+79M", "10Y+95M",  "15Y+154M", "0Y+200M"
};

/**
 * Meopta has never published half-grade settings
 * for their enlargers. They could possible by
 * estimated, but it is probably safer to leave
 * them blank for now.
 */
static const char *GRADE_FILTER_STR_MEOPTA[] = {
    "105Y+0M",
    "85Y+10M", "--", "60Y+20M", "--",
    "40Y+45M", "--", "20Y+60M", "--",
    "10Y+75M", "--", "0Y+200M"
};

const char *contrast_grade_str(contrast_grade_t contrast_grade)
{
    if (contrast_grade >= CONTRAST_GRADE_00 && contrast_grade < CONTRAST_GRADE_MAX) {
        return CONTRAST_GRADE_STR[contrast_grade];
    } else {
        return "";
    }
}

const char *contrast_filter_name_str(contrast_filter_t filter)
{
    if (filter >= CONTRAST_FILTER_REGULAR && filter < CONTRAST_FILTER_MAX) {
        return CONTRAST_FILTER_NAME_STR[filter];
    } else {
        return "";
    }
}

const char *contrast_filter_grade_str(contrast_filter_t filter, contrast_grade_t contrast_grade)
{
    if (contrast_grade >= CONTRAST_GRADE_00 && contrast_grade < CONTRAST_GRADE_MAX) {
        switch (filter) {
        case CONTRAST_FILTER_REGULAR:
            return NULL;
        case CONTRAST_FILTER_DURST_170M:
            return GRADE_FILTER_STR_DURST_170M[contrast_grade];
        case CONTRAST_FILTER_DURST_130M:
            return GRADE_FILTER_STR_DURST_130M[contrast_grade];
        case CONTRAST_FILTER_KODAK:
            return GRADE_FILTER_STR_KODAK[contrast_grade];
        case CONTRAST_FILTER_LEITZ_FOCOMAT_V35:
            return GRADE_FILTER_STR_LEITZ_FOCOMAT_V35[contrast_grade];
        case CONTRAST_FILTER_MEOPTA:
            return GRADE_FILTER_STR_MEOPTA[contrast_grade];
        default:
            return NULL;
        }
    } else {
        return NULL;
    }
}
