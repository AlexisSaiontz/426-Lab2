# 426-Lab2

# Assignment 2: Adding Durability to the Graph Store #

----

## Introduction ##
In a previous lab, we implemented a simple single-node in-memory version of your server, which will store undirected graphs.

In this lab, we've added durability to that graph store. We use a simple logging design, where the graph is stored entirely in main memory, but each incoming update is logged to raw disk. Occasionally the graph is checkpointed to disk and the log is garbage collected. We'll run the system in Google Cloud using a virtual block device in raw mode (i.e. reading and writing blocks directly without first mounting a filesystem on the device).


## Interfaces ##

We're using a simple HTTP based system, which only uses the `HTTP` and `Content-Length` headers. All requests will be in the form:
```HTTP
POST <function_name> HTTP/1.1
<other-headers?>
Content-Length: <length>
<other-headers?>

<length bytes of JSON encoded content>
```

The arguments will be in [JSON](http://www.json.org/). You can use the [Mongoose Library](https://github.com/cesanta/mongoose) in C or C++ for both request handling and JSON decoding if you so desire to. Seach for JSON and HTTP in the header file for more information.


In this lab you will implement the following functions for your undirected-graph store:

   Function    | Method |    Arguments     | Return
-------------- | ------ | ---------------- | ------
 `add_node`    | `POST` | `u64 node_id`    |  `200` on success<br/> `204`if the node already exists <br/> `507`if there is insufficient space for the checkpoint
 `add_edge`    | `POST` | `u64 node_a_id`, `u64 node_b_id`  |  `200` on success<br/>`204` if the edge already exists<br /> `400` if either node doesn't exist, or if `node_a_id` is the same as `node_b_id` <br/> `507`if there is insufficient space for the checkpoint
 `remove_node` | `POST` | `u64 node_id` | `200` on success<br/> `400`if the node does not exist <br/> `507`if there is insufficient space for the checkpoint
 `remove_edge` | `POST` | `u64 node_a_id`, `u64 node_b_id`  |  `200` on success<br/>`400` if the edge does not exist <br/> `507`if there is insufficient space for the checkpoint
 `get_node`    | `POST` | `u64 node_id` | `200` and a boolean JSON field `in_graph` indicating whether the node is in the graph
 `get_edge`    | `POST` | `u64 node_a_id`, `u64 node_b_id`  |  `200` and a boolean JSON field `in_graph` indicating whether the edge is in the graph<br/>`400` of at least one of the vertices does not exist
 `get_neighbors`   | `POST`  | `u64 node_id` | `200` and a list of neighbors[*](#get_neighbors_description) or<br/> `400` if the node does not exist
 `shortest_path`   | `POST`  | `u64 node_a_id`, `uint node_b_id` | `u64` and a field `distance` containing the length of shortest path between the two nodes or<br/>`204` if there is no path <br/>`400` if either node does not exist


Second, we expose a new `checkpoint` command:

   Function    | Method |    Arguments     | Return
-------------- | ------ | ---------------- | ------
 `checkpoint`    | `POST` |     |  `200` on success<br/> `507`if there is insufficient space for the checkpoint
 

In the current server version, the endpoints must be prefixed with `/api/v1/`, see the examples below for more details.

<a name="get_neighbors_description">*</a> Specifically the contents returned from `get_neighbors` must be in the form
```JSON
{
  "node_id": <node_id>,
  "neighbors": [<neighbors>]
}
```

### Example Requests and Responses ###
<a name="examples"></a>
#### add_node ####
Sample Request:
```HTTP
POST www.example.com:8000/api/v1/add_node HTTP/1.1
Content-Length: <length>
Content-Type: application/json

{
  "node_id": 123
}
```
Sample Response:
When request resulted in a newly created node:
```HTTP
HTTP/1.1 200 OK
Content-Length: <length>
Content-Type: application/json

{
  "node_id": 123
}
```
When requested node_id is already in the graph (status code 204, which MUST NOT contain any payload):
```HTTP
HTTP/1.1 204 No Content
<other-headers>
```

When checkpoint area is full:
```HTTP
HTTP/1.1 507 No Content
<other-headers>
```

#### add_edge ####
Sample Request:
```HTTP
POST www.example.com:8000/api/v1/add_edge HTTP/1.1
Content-Length: <length>
Content-Type: application/json

{
  "node_a_id": 1,
  "node_b_id": 2
}
```
Sample Response:
When request resulted in a newly created edge:
```HTTP
HTTP/1.1 200 OK
Content-Length: <length>
Content-Type: application/json

{
  "node_a_id": 1,
  "node_b_id": 2
}
```
When specified edge is already in the graph (status code 204, which MUST NOT contain any payload):
```HTTP
HTTP/1.1 204 No Content
<other-headers>
```
When one or both of the specified nodes are not in the graph, respond with status code 400 Bad Request:
```HTTP
HTTP/1.1 400 Bad Request
<other-headers>

<optional error message payload>
```

When checkpoint area is full:
```HTTP
HTTP/1.1 507 No Content
<other-headers>
```

# Running the graph store

In the command line, accept an additional parameter specifying the device file in addition to the port:

```sh
$ ./cs426_graph_server <port> <devfile>
```
**Note:** To find the disk you add, simply use command `lsblk` to check all of the disks in your vm. Typically, if you follow the above google cloud instruction to create your vm. The disk serving as checkpoint disk is /dev/sdb. To get the permission to open the disk a file and write to it. You need to change the access permisson of this disk using `chmod`.      

Also, support a -f flag that formats/initializes a new deployment (i.e. writes a fresh version of the superblock):

```sh
$ ./cs426_graph_server -f <port> <devfile>
```
**Note:**  As the disk you add is initialized with random bits, so you need to first to format this disk by yourself.

## Protocol Format ##

The first block is a superblock for storing metadata. Here, store the location and size of the log segment. The remainder of the disk is used for the checkpoint. A 10GB disk has 2.5M 4KB blocks. Let block 0 be the metadata superblock. For this lab, we initialized the log size to be the first 2GB (including the superblock), and used the remainder 8GB as the checkpoint area.

Protocol 0: Here we store the current generation number in the metadata block, and always reset the log tail to the start on a checkpoint or a format. The process maintains the current generation number and tail of the log in memory. 

--- On a format, you read the superblock and check if it's valid. If valid, you read the current generation number from the superblock, increment it, and write it back out. If invalid, this store has never been used before; you write a brand new superblock with a generation number of 0. (There are corner cases where the superblock gets corrupted, and formatting it with a generation number of 0 can cause problems due to valid data blocks in the log from the earlier instance; for this homework, assume that superblock corruptions don't occur).

--- On a normal startup, you read the superblock and check if it's valid. If not valid, exit with an error code. If valid, locate the checkpoint using the superblock, and read it in if one exists. Read the generation number from the superblock. Play the log forward until you encounter the first log entry that's invalid or has a different generation number.

--- On a checkpoint call, write out the checkpoint. (You can assume that crashes don't occur during the checkpointing process). Increment the generation number in the superblock. Reset the in-memory tail counter to 0.

`byte 0: 32 bits unsigned int: current generation` (e.g. generation 5)<br/>
`byte 4: 64 bits unsigned int: checksum` (e.g. xor of all 8-byte words in block)<br/>
`byte 12: 32 bits unsigned int: start of log segment` (e.g. block 1)<br/>
`byte 16: 32 bits unsigned int: size of log segment` (e.g. 250K blocks)<br/>

Each 4KB log entry block will have:

`byte 0: 32 bits unsigned int: generation number`<br/>
`byte 4: 32 bits unsigned int: number of entries in block`<br/>
`byte 8: 64 bits unsigned int: checksum`<br/>
`byte 16: first 20-byte entry`<br/>
`byte 36: second 20-byte entry`<br/>
`...`

Each log entry will have a 4-byte opcode (0 for `add_node`, 1 for `add_edge`, 2 for `remove_node`, 3 for `remove_edge`) and two 64-bit node IDs (only one of which is used for `add_node` and `remove_node`).


# Division of work
This assignment was worked on by Alexis Saiontz and Stylianos Rousoglou.
Stelios primarily focused on the log functionality, including storing log
entries and playing the log forward upon normal startup. Alex worked on the
checkpoint command, including the development and implementation of the 
protocol used to persistently store the graph in the checkpoint area.
