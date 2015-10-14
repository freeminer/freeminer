# Distributed under the terms of the GNU General Public License v2

EAPI=5
inherit eutils cmake-utils gnome2-utils user games git-r3

DESCRIPTION="An InfiniMiner/Minecraft inspired game"
HOMEPAGE="http://freeminer.org/"

LICENSE="GPL-3+ CC-BY-SA-3.0"
SLOT="0"
KEYWORDS=""
IUSE="+curl dedicated luajit nls redis +server +sound +spatial +truetype "

RDEPEND="dev-db/sqlite:3
	sys-libs/zlib
	=dev-libs/msgpack-1.1.0
	=dev-libs/jsoncpp-1.6.5
	net-libs/enet
	curl? ( net-misc/curl )
	>=dev-games/irrlicht-1.8-r2
	dev-libs/leveldb
	!dedicated? (
		app-arch/bzip2
		media-libs/libpng:0
		virtual/jpeg
		virtual/opengl
		x11-libs/libX11
		x11-libs/libXxf86vm
		sound? (
			media-libs/libogg
			media-libs/libvorbis
			media-libs/openal
		)
		truetype? ( media-libs/freetype:2 )
	)
	luajit? ( dev-lang/luajit:2 )
	nls? ( virtual/libintl )
	redis? ( dev-libs/hiredis )
	spatial? ( sci-libs/libspatialindex )
	virtual/libiconv"
DEPEND="${RDEPEND}
	>=dev-games/irrlicht-1.8-r2
	nls? ( sys-devel/gettext )"

pkg_setup() {
	games_pkg_setup

	if use server || use dedicated ; then
		enewuser ${PN} -1 -1 /var/lib/${PN} ${GAMES_GROUP}
	fi
}

EGIT_MIN_CLONE_TYPE=shallow
src_unpack() {
	git-r3_fetch "git://github.com/freeminer/freeminer.git" master
	git-r3_checkout "git://github.com/freeminer/freeminer.git"
}

src_prepare() {
#	epatch \
#		"${FILESDIR}"/${P}-as-needed.patch \


	# correct gettext behavior
	if [[ -n "${LINGUAS+x}" ]] ; then
		for i in $(cd po ; echo *) ; do
			if ! has ${i} ${LINGUAS} ; then
				rm -r po/${i} || die
			fi
		done
	fi

	# jthread is modified
	# rm -r src/sqlite || die
	# rm -r src/msgpack-c || die

	# set paths
	sed \
		-e "s#@BINDIR@#${GAMES_BINDIR}#g" \
		-e "s#@GROUP@#${GAMES_GROUP}#g" \
		"${FILESDIR}"/freeminerserver.confd > "${T}"/freeminerserver.confd || die
}

CMAKE_BUILD_TYPE="Release"
#CMAKE_IN_SOURCE_BUILD="1"
src_configure() {
	 local mycmakeargs=(
		$(usex dedicated "-DBUILD_SERVER=ON -DBUILD_CLIENT=OFF" "$(cmake-utils_use_build server SERVER) -DBUILD_CLIENT=ON")
		-DCUSTOM_BINDIR="${GAMES_BINDIR}"
		-DCUSTOM_DOCDIR="/usr/share/doc/${PF}"
		-DCUSTOM_LOCALEDIR="/usr/share/locale"
		-DCUSTOM_SHAREDIR="${GAMES_DATADIR}/${PN}"
		$(cmake-utils_use_enable curl CURL)
		$(cmake-utils_use_enable truetype FREETYPE)
		$(cmake-utils_use_enable nls GETTEXT)
		-DENABLE_GLES=0
		-DENABLE_SYSTEM_MSGPACK=1
		-DENABLE_SYSTEM_JSONCPP=1
		-DENABLE_SPATIAL=$(usex spatial)
		$(cmake-utils_use_enable redis REDIS)
		$(cmake-utils_use_enable sound SOUND)
		$(cmake-utils_use !luajit DISABLE_LUAJIT)
		-DRUN_IN_PLACE=0
		$(use dedicated && {
			echo "-DIRRLICHT_SOURCE_DIR=/the/irrlicht/source"
			echo "-DIRRLICHT_INCLUDE_DIR=/usr/include/irrlicht"
		})
	)

	cmake-utils_src_configure
}

src_compile() {
	cmake-utils_src_compile
}

src_install() {
	cmake-utils_src_install

	if use server || use dedicated ; then
		newinitd "${FILESDIR}"/freeminerserver.initd freeminer-server
		newconfd "${T}"/freeminerserver.confd freeminer-server
	fi

	prepgamesdirs
}

pkg_preinst() {
	games_pkg_preinst
	gnome2_icon_savelist
}

pkg_postinst() {
	games_pkg_postinst
	gnome2_icon_cache_update

	if use server || use dedicated ; then
		elog
		elog "Configure your server via /etc/conf.d/freeminer-server"
		elog "The user \"${PN}\" is created with /var/lib/${PN} homedir."
		elog "Default logfile is /var/lib/freeminer/server.log"
		elog
	fi
}

pkg_postrm() {
	gnome2_icon_cache_update
}
