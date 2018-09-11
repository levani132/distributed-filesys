# Net Raid File System

## ლოგირება

სანამ უშუალოდ პროექტის აღწერაზე გადავალ, დავიწყოთ ლოგერის აღწერით. ლოგერი წარმოადგენს რაღაც სტატიკური ობიექტის მსგავს სტრუქტურას და ქვია კონსოლი (ჯავასკრიპტთან მუშაობის გამოცდილების გამო :დ). კონსოლს აქვს ორი სპეციფიური მეთოდი (set/unset)_file, რომლის საშუალებითაც შეგვიძლია დავუსეტოთ ფაილი კონსოლს რომელშიც გადაამისამართებს ლოგებს.

ყველა ეს მეთოდი ყოველი მესიჯის წინ ბეჭდავს დაბეჭდვის დროს მოცემულ ფორმატში:

[Sun Jun 10 12:30:37 2018]

კონსოლს გააჩნია სამი ძირითადი მეთოდი ლოგირებისთვის, ესენია log, logger, logger_error.

log წარმოადგენს სტანდარტული printf-ის მსგავს ფუნქციას, გადაეცემა ფორმატ სტრინგი და ცვლადები და ბეჭდავს მათ. მისი განსაკუთრებულობა მდგომარეობს მხოლოდ იმაში რომ დროს ბეჭდავს მესიჯის წინ.

logger წარმოადგენს მეთოდს რომელიც ბეჭდავს მესიჯს წინ დისკის სახელს და სერვერს. თუ პირველი ორი პარამეტრიდან რომელიმე NULL პოინტერია, ამ პარამეტრს უბრალოდ გამოტოვებს და არ დაბეჭდავს.

logger_error კი მეთოდია რომელსაც გადაეცემა ფაილის სახელი და ხაზის ნომერი საიდანაც დაბეჭდვა ხდება და მესიჯის წინ ამ პარამეტრებს წერს.

კონსოლს ასევე გააჩნია ორი პრეპროცესორის მიერ ჩასანაცვლებელი აღნიშვნები, LOGGER და LOGGER_ERROR, პირველი თუ საჭირო ადგილზე მოხდა გამოძახება ხვდება დისკის სახელს და სერვერს, და იუზერს პირდაპირ ფორმატ მესიჯის და პარამეტრების დაწერა უწევს ხოლო მეორე ხვდება ფაილის სახელს და ხაზს და მასაც მხოლოდ ფორმატ მესიჯი და პარამეტრებიღა ჭირდება. ისინი იძახებენ შესაბამისი სახელების მეთოდებს.

## არქიტექტურა

პროგრამის იმპლემენტაციის ჩემი ვარიანტი შედგება რა თქმა უნდა კლიენტის და სერვერის ნაწილისგან, მათი სოურს ფაილები დაყოფილია შესაბამის ფოლდერებში. გარდა ამ განცალკევებული ფაილებისა, პროგრამას გააჩნია საერთო ფაილები (როგორებიცაა message.c, protocol.c, logger.c). საერთო ფაილები დაბილდვის დროს უბრალოდ ორივე მხარის სოურსებში არის გაწერილი, რაც რა თქმა უნდა განაბირობებს იმას, რომ პროდუქტად ორ დამოუკიდებელ პროგრამას ვიღებთ.

საერთო ფაილების სახელებიდან გამომდინარეც შეგვიძლია მივხვდეთ რომ ისინი ერთგვარ პროტოკოლს წარმოადგენენ სერვერისა და კლიენტის ურთიერთობისთვის, სადაც მესიჯი წარმოადგენს პროტოკოლისთვის აუცილებელ სტრუქტურას რომელიც ნებისმიერ კავშირს უნდა სდევდეს თავს, მათ შესახებ მოგვიანებით უფრო დაწვრილებით დავწერ. რაც შეეხება ლოგერს, ის უბრალოდ სტრუქტურირებულად დასალოგად არის საჭირო და ორივე პროგრამაში შედის უბრალოდ კოდის გამეორება რომ არ გამოსულიყო იმისთვის.

ზემოთ აღწერილი სტრუქტურიდან გამომდინარე შეგვიძლია ზოგადად, მოკლედ აღვწეროთ პროექტის არქიტექტურა: კლიენტში გადატვირთული fuse მეთოდები ახდენენ მოცემული პროტოკოლის საშუალებით სერვერს მოთხოვონ ინფორმაცია სერვერზე არსებული ფაილური სისტემის შესახებ, ხოლო სერვერზე ფაილური სისტემა გამართულია ლინუქსის სტანდარტული ფაილური syscall-ების საშუალებით.

### საერთო

typedef struct request {
    struct message* (*msg_msg)(struct message* message_to_send, const char* server);
    void* (*msg_data)(struct message* message_to_send, const char* server);
    struct message* (*data_msg)(struct message* message, const char* data, int size, const char* server);
    long (*msg_status)(struct message* message_to_send, const char * server);
    long (*data_status)(struct message * to_send, const char* data, int size, const char* server);
    long (*ping)(const char* server);
} *Request; // სტრუქტურა მოცემული პროტოკოლით სერვერთან ურთიერთობისთვის საჭირო მეთოდებისთვის.

typedef struct protocol {
    int (*open_server)(const char * ip, int port);
    void* (*get_data)(int sock, int size);
    struct message* (*get_message)(int sock);
    int (*send_data)(int sock, void* data, int size);
    int (*send_message)(int sock, struct message* message);
    int (*send_status)(int sock, long status);
    struct request request;
} *Protocol; // პროტოკოლის სტრუქტურა, მეთოდები გამოიყენება სერვერზე მოთხოვნების მისაღებად და შესამაბისი პასუხებით დასაკმაყოფილებლად, ასევე გააჩნია რექუესთ სტრუქტურა.

struct message {
    int function_id;
    long status;
    int wait_for_message;
    size_t size;
    off_t offset;
    mode_t mode;
    dev_t dev;
    char small_data[PATH_MAX];
}; // ერთგვარი ჰედერ ფაილი ყველა მოთხოვნისთვის.

struct getattr_ans {
    int retval;
    struct stat stat;
}; // ატრიბუტების და სტატუსის ერთად მისაღებად საჭირო სტრუქტურა.

enum function_id {
    fnc_nothing,
    fnc_ping,
    fnc_opendir,
    fnc_readdir,
    fnc_getattr,
    fnc_open,
    fnc_read,
    fnc_write,
    fnc_utime,
    fnc_truncate,
    fnc_release,
    fnc_releasedir,
    fnc_mknod,
    fnc_mkdir,
    fnc_rename,
    fnc_unlink,
    fnc_rmdir,
    fnc_restore,
    fnc_readall,
    fnc_restoreall
}; // ენუმერაცია რათა სერვერი მიხვდეს რა მოთხოვნა შემოდის.

სერვერისა და კლიენტის ურთიერთობისთვის გამოიყენება სამი ძირითადი პრინციპი და მათგან გამომდინარე ორი დამატებითი მეთოდი:

1. ვაგზავნით მესიჯს და ვიღებთ მესიჯს,
2. ვაგზავნით მესიჯს და ვიღებთ ნებისმიერი ზომის დიდ ინფორმაციას,
3. ვაგზავნით ნებისმიერი ზომის დიდ ინფორმაციას და ვიღებთ მესიჯს,

4. ვაგზავნით მესიჯს და ვიღებთ სტატუსს,
5. ვაგზავნით ნებისმიერი ზომის დიდ ინფორმაციას და ვიღებთ სტატუსს.

მესიჯის გაგზავნა ხდება ყოველთვის ერთი მოთხოვნით და მესიჯს გააჩნია საკმარისი ცვლადები რათა მარტივი ინფორმაციის გადაცემის საჭიროება დააკმაყოფილოს, ხოლო დიდი ინფორმაციის გასაგზავნად იგზავნება მესიჯი რომელსაც გააჩნია wait_for_message ცვლადი რომელშიც წერია რამხელა ინფორმაციას ველოდოთ, შემდეგ უკვე იგზავნება რამხელა მესიჯიც გვინდა.

ყოველი მოთხოვნისთვის სოკეტი კლიენტსა და სერვერს შორის ახლიდან იხსნება და იკეტება რაც პროტოკოლს აძლევს საშუალებას მიიღოს მოთხოვნები რამდენიმე კლიენტისგან ერთდროულად.

შეცდომების შემთხვევაში, ყველგან სადაც შესაძლებელია რომ შეცდომა მოხდეს ლოგავს ამ შეცდომაზე ინფორმაციას ლოგერის მიერ მოწოდებული ერორის დამლოგავი მეთოდის საშუალებით და შესაბამის სტატუსს უბრუნებს გამომძახებელს.

### კლიენტის მხარე

გამოყენებული სტრუქტურები

struct fd_wrapper {
    long fd;
    long server_fd;
};  // ლოკალური დესკრიპტორის სერვერის დესკრიპტორთან დასამეპად საჭირო სტრუქტურა.

struct server {
    char name[20];
    int state;
    struct fd_wrapper* fds;
    int n_fds;
}; // სერვერის სახელის, ამჟამინდელი მდგომარეობის და დესკრიპტორების შესანახი სტრუქტურა.

struct storage {
    char * diskname;
    char * mountpoint;
    int raid;
    int n_servers;
    struct server * servers;
    char * hotswap;
}; // დისკის შესანახი სტრუქტურა, რომელსაც ჩემ შემთხვევაში storage ქვია.

struct config {
    char * errorlog;
    int cache_size;
    char * cache_replacement;
    int timeout;
    int n_storages;
    struct storage * storages;
}; // კონფიგურაციის სტრუქტურა, რომელშიც ინახება მთელი კონფიგურაციის ცვლადები.

typedef struct cache {
    char* (*read)(const char* path, size_t size, off_t offset);
    int (*write)(const char* path, const char* buf, size_t size, off_t offset);
    int (*rename)(const char* path, const char* new_path);
} *Cache; // ქეშ სტრუქტურა რომელიც ინახავს ჩაწერის წაკითხვის და ქეშის ჩანაწერზე მიმთითებლის შეცვლის მეთოდებს.

struct entry {
    char* data;
    size_t size;
    char path[PATH_MAX];
    struct entry* older;
    struct entry* newer;
}; // ქეშის double linked list-ის ენთრის შესანახად საჭირო სტრუქტურა

Request - სერვერთან მოთხოვნების გასაკეთებლად საჭირო სტრუქტურა.

კლიენტის მხარეს ხდება კონფიგურაციის ფაილის დაპარსვა და კონფიგურაციის სტრუქტურაში შენახვა, დისკების და სერვერების ინიციალიზაციასთან ერთად. პროგრამა იფორკება იმდენად რამდენი დისკიც გვაქ და user_data-ს საშუალებით გადაეცემა fuse-ს. ფუზი ახდენს სტანდარტული ფუნქციების, წაკითხვების განხორციელებას პირველი სერვერიდან, თუმცა ახდენს ყველა სერვერზე გახსნას და დახურვას ფაილების, რათა ჩაწერის დროს ყველა სერვერზე ასახოს ცვლილება. მიმთითებლების შენახვა ხდება თითოეული სერვერისთვის სერვერის სტრუქტურაში ლოკალურად შემოტანილ დესკრიპტორზე დამეპვით. ლოკალური დესკრპიტორი fuse-ის ფაილის ინფორმაციას გადაეცემა და მასზე წცდომა გვაქვს კითხვის, წერის და დახურვის დროს, შესაბამისად ნებისმიერი ამ გამოძახებებისას გავიგოთ თუ სერვერზე ამ ფაილს რეალურად რა დესკრიპტორი ქონდა.

კლიენტის მხარეს ყოველი სერვერის მეთოდის გამოძახების წინ და შემდეგ ილოგება მესიჯი თუ რომელმა დისკმა, რომელ სერვერზე რა მეთოდის გამოძახება როდის დაიწყო და როდის დაამთავრა.

კლიენტს გააჩნია ასევე ქეშირების მოდული, რომელშიც პირველი წაკითხვისას ინახება ფაილის დატა, ყოველ შემდეგ წაკითხვაზე კი სერვერთან მიმართვა აღარ ხდება. ნებისმიერი ცვლილება რომელსაც სერვერზე ახორციელებს კლიენტი ასევე ქეშშიც ხდება. ქეშის დასაწერად მარტივი ალგორითმი გამოვიყენე, უბრალოდ გამოვუყოფ მაქსიმალურ მეხსიერებას რაც ჭირდება ამ ფაილს და ვუწერ ამ მეხსიერებაში ფაილის იმ ფრაგმენტს რომელიც ჭირდება, თუ ახალი ჩაწერისას უფრო დიდი ზომის რაიმეს წერს თავიდან ვუყოფ ადგილს, გარდა ამისა კონფიგში შემოსული მაქსიმალური ქეშის მეხსიერების გადაჭარბების შემთხვევაში ნებისმიერ დროს როცა ქეშისგან მოთხოვნილი მეხსიერება იზრდება, ვაკეთებ ყველაზე ძველი გამოყენებული ინფორმაციის წაშლას (rlu). ამის საშუალებას მაძლევს ორმაგად ბმული სია, რომლის თავსაც ვშლი ყოველთვის ხოლო ახალ ჩამატებულ ან შეცვლილ ან წაკითხულ ინფორმაციას ვშლი სადაც არ უნდა იყოს ამჟამად და გადმომაქ კუდში.

### სერვერის მხარე

typedef struct FileManager {
    char root_path[PATH_MAX];
    intptr_t (*opendir) (const char * path);
    struct message* (*open) (const char * path, int flags);
    char* (*readdir) (intptr_t dp, char* path);
    struct getattr_ans* (*getattr)(const char * path);
    char* (*read)(int fd, size_t size, off_t offset);
    int (*truncate)(char* path, off_t size);
    int (*utime)(const char *__file, struct utimbuf *__file_times);
    int (*mknod)(const char *path, mode_t mode, dev_t dev);
    int (*mkdir)(const char *path, mode_t mode);
    int (*rename)(const char *path, char *newpath);
    int (*unlink)(const char *path);
    int (*rmdir)(const char *path);
    int (*write)(const char* path, int fd, void* data, size_t size, off_t offset);
    int (*restore)(const char* path, const char* server);
    char* (*readall)(const char* path);
    int (*restoreall)(const char* path, const char* server, int first);
    void (*log)(const char* msg);
} *FileManager;

სერვერის მხარეს გამოყენებული მაქვს მხოლოდ ერთი ახალი სტრუქტურა, ფაილების მენეჯერი, რომელსაც უბრალოდ აქვს მეთოდები რომლებიც თავის მხრივ რეალური syscall-ების wrapper-ებია, უბრალოდ ინფორმაციას ისე ამუშავებს და ისე აბრუნებს როგორც პროტოკოლს ყველაზე მეტად მოუხერხდება შემდეგ რომ მარტივად გააგზავნოს. სერვერი გარდა იმისა რომ მოთხოვნების მიღებისას ყველა კლიენტისთვის თავიდან იღებს მოთხოვნებს მათ შეუფერხებლად შესასრულებლად იყენებს epoll API-ს.

სერვერს ასევე გააჩნია ჰეშირებისთვის საჭირო მეთოდები ცალკე ფაილში, რომელიც იღებს ფაილის სახელს და წერს მისთვის ჰეშს ან აბრუნებს მის ჰეშს.

სერვერიც, როგორც კლიენტი ყოველ ახალი შემოსული მოთხოვნისას წერს მოთხოვნის სახელს. გარდა ამისა კლიენტი ყოველი ახალი მიერთებული კლიენტის იპ-ს წერს და ყოველი მოთხოვნის ბოლოს წერს რომ დაიხურა სოკეტი და რომ ელოდება ახალ მომხმარებელს. ამას ის console.log მეთოდის საშუალებით აკეთებს.

### ფაილების შენახვის პრინციპი

ფაილების შესანახად კლიენტს გააჩნია მხოლოდ raid 1 შენახვის მხარდაჭერა.

raid 1-სთვის სერვერები ყოველი ჩაწერისას ახდენენ ფაილების ჰეშირებას და მათი ჰეშების გაფართოებულ ატრიბუტებში ჩაწერას. ყოველი open გამოძახებისას კი სერვერი ამოწმებს რამდენად სწორია ფაილის ჰეში ამჟამად მის ატრიბუტში არსებული ჰეშის და კლიენტს უბრუნებს სწორია თუ არა, ბოლო ცვლილება როდის მოხდა ამ ფაილზე და ამჟამინდელ ჰეშს. თუ სერვერს არასწორად აქვს ჰეში შენახული ანუ ამ სერვერზე ფაილი დაზიანებულია და ცდილობს აღადგინოს მასზე დაბალი ინდექსის მქონე სერვერიდან, პირველი სერვერისთვის ასეთი ბოლო სერვერია. თუ ორივე სერვერზე დაზიანებულია ფაილი მაშინ ეს ფაილი ზოგადადაც დაზიანებულად ცხადდება. თუ არცერთზე არ არის დაზიანებული მაგრამ დაბრუნებული ჰეშები არ ემთხვევა მაშინ რომელზეც უფრო გვიან მოხდა ცვლილება იმ სერვერიდან ხდება მეორეზე აღდგენის გაკეთება.

თუ რომელიმე სერვერი გაითიშა რაც დგინდება მოთხოვნის გაკეთებისას თუ ვერ შეძლო, სერვერის მდგომარეობა გადადის DOWN-ში(ამ დროს სერვერზე მოთხოვნები არ ხდება), მის შემდგომ მყოფი სერვერი ჩამოინაცვლებს ამ სერვერის ინდექსზე და კლიენტი იძახებს reconnect მეთოდს რომელიც ცალკე სრედს უშვებს რომელიც კონფიგურაციაში მოცემული დროის მანძილზე ცდილობს დაუკავშირდეს თავიდან ამ სერვერს პინგ გამოძახების საშუალებით, თუ გამოუვიდა ამ სერვერის მდგომარეობა გადაყავს STARTING მდგომარეობაში, თუ არ გამოუვიდა უბრალოდ უშლის დესკრიპტორებს და სერვერის სახელს ცვლის სვეპ სერვერით. როგორც კი ეს (ან ჩანაცვლებული) სერვერი რაიმის გამოძახებას დააპირებს, თუ STARGING მდგომარეობაშია მაშინ მასზე დაბალი ინდექსის მქონე სერვერიდან იწყებს სრულ დუბლირებას. ამით ყველა ცვლილება რაც გაუთიშავ სერვერზე ყოფილა ასევე გადმოვა აღდგენილ ან ჩანაცვლებულ სერვერზე. ამის შემდეგ კლიენტი აგრძელებს ჩვეულებრივ რეჟიმში მუშაობას.

## პროექტის გაშვება

პროექტს ჭირდება ფუზის და openssl ბიბლიოთეკების დაყენება, ამისთვის საჭიროა გავუშვათ ბრძანებები

sudo apt install libpcap-dev libssl-dev
sudo apt install pkg-config libfuse-dev

პროექტის დასაბილდად უბრალოდ ვუშვებთ make ბრძანებას, ძველი დაბილდული პროგრამის გასაწმენდად კი make clean-ს. ის ქმნის საჭირო ფოლდერებს პროექტში არსებული კონფიგურაციისთვის.

დებაგირებისთვის კი ვუშვებთ ბრძანებას make debug, რომელიც აკეთებს იგივეს და ასევე უწერს -g ფლეგს gdb-სთვის.

კლიენტის გასაშვებად აუცილებელია კონფიგურაციის ფაილის შემდეგი ფორმატით მოწოდება და შესაბამისი mountpoint ფოლდერების არსებობა

errorlog = error.log
cache_size = 1024M
cache_replacment = rlu
timeout = 10

diskname = STORAGE1
mountpoint = test1
raid = 1
servers = 127.0.0.1:10001, 127.0.0.1:10002
hotswap = 127.0.0.1:10003

diskname = STORAGE2
mountpoint = test2
raid = 1
servers = 127.0.0.1:10004, 127.0.0.1:10005
hotswap = 127.0.0.1:10006

...

სერვერის გასაშვებად საჭიროა თავისუფალი პორტი მოცემულ ip-ზე და ფოლდერი რომელშიც მოახდენს ფაილების შენახვას სერვერი.

სერვერის გასაშვებად პროექტის ფოლდერიდან ვუშვებთ ბრძანებას $ build/server <ip> <port> <storage-folder>

საწყისი კონფიგურაციისთვის შესაბამისად კი შეგვიძლია გავუშვათ სერვერები ასე

build/server 127.0.0.1 10001 test1
build/server 127.0.0.1 10002 test2
build/server 127.0.0.1 10003 test3

კლიენტის გაშვება პროექტის ფოლდერიდან ხდებას ბრძანებით $ build/client config // ან სხვა კონფიგურაციის ფაილი