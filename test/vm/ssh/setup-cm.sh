#!/bin/bash
# Copy ControlMaster ssh configs when the OS supports it

cp -f /backrest/test/vm/ssh/config-cm /home/vagrant/.ssh/config
cp -f /backrest/test/vm/ssh/config-cm /home/backrest/.ssh/config
