Hashtable lookup over RDMA
===


This example implements two parts of the hashtable lookup process:

- The **hashserver** prepares a local persistent memory and registers it as a reading 
source. The local persistent memory contains the hashtable data populated by 
./writehash. The server waits for client's connection request. After the connection
is established, the client just waits for the server to disconnect.

- The **singlehashclient** registers a volatile memory region as a read destination. It 
gets the mmap address of the server through connection's private data. User gives the
file path that contains the list of keys to lookup. Client parses the keys into an array
and lookups each key. For each key, client computes the hash index and uses it as an 
offset to read 2 KB from server's remote memory and waits for its completion. The 2 KB 
segment guarantees to contain the key, if the key exists. The client checks first the 
metadata of the first bucket in the segment. If the bucket is empty, the key does not 
exist. If the bucket is valid, it checks the key. If the key does not match, the client 
gets the hopinfo from metadata and uses it to traverse through the possible relocated key 
locations in the segment. Once found, it checks for the next key.
**The multihashclient** is the multithreaded version of singlehashclient. This client
can connect to as many as servers the user inputs and uses pthread for multithreading. 
The client uses the hash function to find on which server the key is located and then 
uses jenkins hash to find the hashindex inside each server. It also uses the **hashthread** 
header file to pass the arguments to thread functions.


Supporting source files:

- The **writehash** reads key data from the YCSB trace files and populates the hopscotch
hashtable. Then it writes the hopscotch hashtable data to pmem. It uses 64 B buckets: 
5 B metadata (1 B bucket info and 4 B hopinfo), 32 B key and 27 B value. The hopscotch 
table uses H = 32, where a key can be relocated to nearest H-1 buckets.

- The **readhash** reads key data from the YCSB trace files and lookups the key from
pmem that contains the hashtable.

- The **multiclient** show example of how one client connect and reads from multiple
servers.



## Usage

```bash
[user@server]$ ./hashserver $server_address $port [<pmem-path>]
```

```bash
[user@client]$ ./multihashclient [<key-path>] $number_of_servers $server_address $port 
```

```bash
[user@client]$ ./singlehashclient [<key-path>] $server_address $port 
```

where `<pmem-path>` can be:
  - a Device DAX (`/dev/dax0.0` for example) or
  - a file on File System DAX (`/mnt/pmem/file` for example).
