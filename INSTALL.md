# pg_backrest installation

## parameters

## configuration

## sample ubuntu 12.04 install

apt-get update
apt-get upgrade (reboot if required)

apt-get install ssh
apt-get install cpanminus
apt-get install postgresql-9.3
apt-get install postgresql-server-dev-9.3

cpanm JSON
cpanm Moose
cpanm Net::OpenSSH
cpanm DBI
cpanm DBD::Pg
cpanm IPC::System::Simple

Create the file /etc/apt/sources.list.d/pgdg.list, and add a line for the repository
deb http://apt.postgresql.org/pub/repos/apt/ precise-pgdg main

wget --quiet -O - https://www.postgresql.org/media/keys/ACCC4CF8.asc | sudo apt-key add -
sudo apt-get update


For unit tests:

create backrest user
setup trusted ssh between test user account and backrest
backrest user and test user must be in the same group
