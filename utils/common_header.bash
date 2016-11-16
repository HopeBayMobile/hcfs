# fix CI error
if [ $(id -u) -eq 0 ]; then
	umask 000
fi

ErrorReport()
{
	local script="$1"
	local parent_lineno="$2"
	local code="${3:-1}"
	eval printf %.0s- '{1..'"${COLUMNS:-$(tput cols)}"\}; echo
	echo "Error is near ${script} line ${parent_lineno}. Return ${code}"
	local Start
	local Point
	if [[ $parent_lineno -lt 1 ]]; then
		Start=$parent_lineno
		Point=1
	else
		Start=$((parent_lineno-2))
		Point=3
	fi
	local End=$((parent_lineno+2))
	cat -n "${script}" \
		| sed -n "${Start},${End}p" \
		| sed $Point"s/^  / >/"
	eval printf %.0s- '{1..'"${COLUMNS:-$(tput cols)}"\}; echo
	exit "${code}"
}

install_pkg()
{
	[[ "$-" = *"x"* ]] && flag_x="-x" || flag_x="+x"
	set +x
	for pkg in $packages;
	do
		if ! dpkg -s $pkg >/dev/null 2>&1; then
			install="$install $pkg"
		fi
	done
	install="$(echo -e "${install:-} ${force_install:-}" | sed -e 's/^[[:space:]]*//' -e 's/[[:space:]]*$//')"
	if [ -n "$install" ]; then
		echo -e "\nError: Require packages: $install"
		if [ "$1" = "check_pkg" ]; then
			return 1
		fi
		sudo apt-get update || :
		sudo apt-get install -y $install $force_install
		packages=""
		install=""
		force_install=""
	fi
	eval set $flag_x
	for i in ${post_pkg_install:-}
	do
		echo Running post-install tasks: $i
		$i
	done
	if [ "${DOCKER_BUILD:-0}" -eq 1 ]; then
		sudo rm -rf /tmp/* /var/tmp/*
	fi
}

check_script_changes()
{
	script_path=$(dirname $1)
	script_path=$(cd $script_path; pwd)
	THIS_SCRIPT="$script_path/$(basename $1)"
	checkfile=$(dirname ${THIS_SCRIPT})/.done_$(basename ${THIS_SCRIPT})
	if [ ! -z "${setup_dev_env_mode:=}" ]; then
		checkfile+="_$setup_dev_env_mode"
	fi
	checkfile+=".md5"
	echo checkfile $checkfile

	if md5sum -c "$checkfile" >/dev/null 2>&1; then
		exit
	else
		rm -f "$checkfile"
	fi
}

commit_script_changes()
{
	checkfile="`dirname ${THIS_SCRIPT}`/.done_`basename ${THIS_SCRIPT}`.md5"
	if [ ! -z "$setup_dev_env_mode" ]; then
		checkfile="`dirname ${THIS_SCRIPT}`/.done_`basename ${THIS_SCRIPT}`_$setup_dev_env_mode.md5"
	fi

	commit_file="${THIS_SCRIPT}"
	if [ "$#" -eq 1 ]; then
		commit_file="$1"
	fi

	touch "$checkfile"
	md5sum "$commit_file" | sudo tee -a "$checkfile"
	sudo chown --reference="$commit_file" "$checkfile"
}

# Main source

# Enable debug log only if verbose on
if ${CI_VERBOSE:-false}; then set -x; else set +x; fi

# Enable error trace
trap 'ErrorReport "${BASH_SOURCE[0]}" ${LINENO} $?' ERR
set -o pipefail  # trace ERR through pipes
set -o errtrace  # trace ERR through 'time command' and other functions
set -o nounset   ## set -u : exit the script if you try to use an uninitialised variable
set -o errexit   ## set -e : exit the script if any statement returns a non-true return value
