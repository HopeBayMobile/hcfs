umask 000 # fix CI error

function script_error_report() {
	set +x
	local script="$1"
	local parent_lineno="$2"
	local message="$3"
	local code="${4:-1}"
	echo "Error near ${script} line ${parent_lineno}; exiting with status ${code}"
	if [[ -n "$message" ]] ; then
		echo -e "Message: ${message}"
	fi
	exit "${code}"
}

function install_pkg (){
	[[ "$-" =~ "x" ]] && flag_x="-x" || flag_x="+x"
	set +x
	for pkg in $packages;
	do
		if ! dpkg -s $pkg >/dev/null 2>&1; then
			install="$install $pkg"
		fi
	done
	install="$(echo -e "$install $force_install" | sed -e 's/^[[:space:]]*//' -e 's/[[:space:]]*$//')"
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
	for i in $post_pkg_install
	do
		echo === $i
		$i
	done
}

function check_script_changes()
{
	script_path=$(dirname $1)
	script_path=$(cd $script_path; pwd)
	THIS_SCRIPT="$script_path/$(basename $1)"
	checkfile="`dirname ${THIS_SCRIPT}`/.done_`basename ${THIS_SCRIPT}`.md5"
	if [ ! -z "$setup_dev_env_mode" ]; then
		checkfile="`dirname ${THIS_SCRIPT}`/.done_`basename ${THIS_SCRIPT}`_$setup_dev_env_mode.md5"
	fi

	if md5sum -c "$checkfile" >/dev/null 2>&1; then
		exit
	else
		rm -f "$checkfile"
	fi
}

function commit_script_changes()
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
# Enable error trace
trap 'script_error_report "${BASH_SOURCE[0]}" ${LINENO}' ERR
set -e -o errtrace
# Enable debug log only if verbose on
if ${CI_VERBOSE:-false}; then set -x; else set +x; fi
