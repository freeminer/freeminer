# Copyright 1999-2014 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header: /var/cvsroot/gentoo-x86/games-action/minetest_game/minetest_game-0.4.10.ebuild,v 1.2 2014/10/15 11:47:04 nimiux Exp $

EAPI=5
inherit vcs-snapshot games

DESCRIPTION="The main game for the Minetest game engine"
HOMEPAGE="http://freeminer.org/"
SRC_URI="https://github.com/freeminer/default/tarball/${PV} -> ${P}.tar.gz"

LICENSE="GPL-2 CC-BY-SA-3.0"
SLOT="0"
KEYWORDS="amd64 ~x86"
IUSE=""

RDEPEND="~games-action/freeminer-${PV}[-dedicated]"

src_unpack() {
	vcs-snapshot_src_unpack
}

src_install() {
	insinto "${GAMES_DATADIR}"/freeminer/games/default
	doins -r mods menu
#	doins game.conf freeminer.conf
	doins game.conf minetest.conf

	dodoc README.txt game_api.txt

	prepgamesdirs
}
