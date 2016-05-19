# Copyright (c) 2016 Aleksandr Borisenko
# Distributed under the terms of the GNU General Public License v2

# Rotate the log files created by WRN Daemon.

/var/log/wrnd/*.log {
	weekly
	rotate 4
	missingok
	notifempty
	sharedscripts
	postrotate
		/etc/init.d/wrnd reload >/dev/null 2>&1 || true
	endscript
}
