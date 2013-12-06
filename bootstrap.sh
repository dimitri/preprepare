#!/usr/bin/env bash

# PostgreSQL
sidsrc=/etc/apt/sources.list.d/sid-src.list
echo "deb-src http://ftp.fr.debian.org/debian/ sid main" | sudo tee $sidsrc

pgdg=/etc/apt/sources.list.d/pgdg.list
pgdgkey=https://www.postgresql.org/media/keys/ACCC4CF8.asc
echo "deb http://apt.postgresql.org/pub/repos/apt/ wheezy-pgdg main" | sudo tee $pgdg

wget --quiet -O - ${pgdgkey} | sudo apt-key add -

sudo apt-get update
sudo apt-get install -y postgresql-server-dev-all rsync devscripts

make -C /vagrant unsign-deb
