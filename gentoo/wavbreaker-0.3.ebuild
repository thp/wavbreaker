DESCRIPTION="wavbreaker/wavmerge GTK2 utility to break or merge WAV file"
HOMEPAGE="http://huli.org/wavbreaker/"
SRC_URI="http://huli.org/wavbreaker/${P}.tar.gz"

KEYWORDS="x86"
SLOT="0"
LICENSE="GPL-2"

IUSE=""
RDEPEND="dev-libs/libxml2
         >=x11-libs/gtk+-2*"

DEPEND="virtual/glibc"

src_compile () {
	econf || die
	emake || die
}

src_install () {
	make DESTDIR=${D} install
	dodoc ChangeLog COPYING INSTALL README NEWS
}

