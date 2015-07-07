#!/bin/bash
# Setup backrest test user and trusted SSH access between vagrant and test users

# Copy ssh keys for vagrant user
cp /backrest/test/vm/ssh/config /home/vagrant/.ssh
cp /backrest/test/vm/ssh/id_rsa /home/vagrant/.ssh
cat /backrest/test/vm/ssh/id_rsa.pub >> /home/vagrant/.ssh/authorized_keys
chown -R vagrant:vagrant /home/vagrant/.ssh
chmod 700 /home/vagrant/.ssh
chmod 600 /home/vagrant/.ssh/*

# Create the backrest user and copy ssh keys
adduser --ingroup=vagrant --disabled-password --gecos "" backrest
mkdir /home/backrest/.ssh
cp /backrest/test/vm/ssh/config /home/backrest/.ssh
cp /backrest/test/vm/ssh/id_rsa /home/backrest/.ssh
cp /backrest/test/vm/ssh/id_rsa.pub /home/backrest/.ssh/authorized_keys
chown -R backrest:vagrant /home/backrest/.ssh
chmod 700 /home/backrest/.ssh
chmod 600 /home/backrest/.ssh/*
