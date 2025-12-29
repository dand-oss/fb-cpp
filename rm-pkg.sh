declare -r pkg="$(basename "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)")"
rm -rf ${ASV_AF_PORTS}/build/${pkg}-build-${aplatform} ${ASV_PLAT_PORTS}/${pkg}
