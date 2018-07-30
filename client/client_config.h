
struct storage {
    char * diskname;
    char * mountpoint;
    int raid;
    int n_servers;
    char ** servers;
    char * hotswap;
};

struct config {
    char * errorlog;
    int cache_size;
    char * cache_replacement;
    int timeout;
    int n_storages;
    struct storage * storages;
};

void config_init(struct config * config, char * filename);
void config_dest(struct config * config);