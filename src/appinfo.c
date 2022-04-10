/* wavbreaker - A tool to split a wave file up into multiple wave.
 * Copyright (C) 2022 Thomas Perl
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "appinfo.h"

#include <config.h>

#include "gettext.h"

const char *
appinfo_copyright()
{
    return "Copyright (C) 2002-2007 Timothy Robinson\n"
           "Copyright (C) 2006-2008, 2012, 2015-2016, 2018-2019, 2022 Thomas Perl";
}

const char *
appinfo_description()
{
    return _("Split a wave file into multiple chunks");
}

const char *
appinfo_url()
{
    return "https://wavbreaker.sourceforge.io/";
}

const char **
appinfo_authors()
{
    static const char *AUTHORS[] = {
        "Timothy Robinson <tdrobinson@huli.org>",
        "Thomas Perl <m@thp.io>",
    };

    return AUTHORS;
}

const char *
appinfo_version()
{
    return VERSION;
}
