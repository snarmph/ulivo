#include "http.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// #include "os.h"
#include "tracelog.h"

#include "vec.h"

#ifdef _WIN32
    #define stricmp _stricmp
#else
    #include <strings.h> // strcasecmp
    #define stricmp strcasecmp
#endif

// == INTERNAL ================================================================

static void _setField(vec(http_field_t) *fields_vec, const char *key, const char *value) {
    vec(http_field_t) fields = *fields_vec;

    for (uint32 i = 0; i < vecLen(fields); ++i) {
        if (stricmp(fields[i].key, key) == 0) {
            char **curval = &fields[i].value;
            usize curlen = strlen(*curval);
            usize newlen = strlen(value);
            if(newlen > curlen) {
                *curval = (char *)realloc(*curval, newlen + 1);
            }
            memcpy(*curval, value, newlen);
            (*curval)[newlen] = '\0';
            return;
        }
    }

    // otherwise, add it to the list
    http_field_t field;
    usize klen = strlen(key);
    usize vlen = strlen(value);
    field.key = (char *)malloc(klen + 1);
    field.value = (char *)malloc(vlen + 1);
    memcpy(field.key, key, klen);
    memcpy(field.value, value, vlen);
    field.key[klen] = field.value[vlen] = '\0';

    vecAppend(*fields_vec, field);
}

static void _parseFields(vec(http_field_t) *fields, str_istream_t *in) {
    strview_t line;

    do {
        line = istrGetview(in, '\r');

        usize pos = strvFind(line, ':', 0);
        if(pos != STRV_NOT_FOUND) {
            strview_t key = strvSub(line, 0, pos);
            strview_t value = strvSub(line, pos + 2, SIZE_MAX);

            char *key_str = NULL;
            char *value_str = NULL;

            key_str = strvCopy(key).buf;
            value_str = strvCopy(value).buf;

            _setField(fields, key_str, value_str);

            free(key_str);
            free(value_str);
        }

        istrSkip(in, 2); // skip \r\n
    } while(line.len > 2);
}

// == HTTP STATUS =============================================================

const char *httpGetStatusString(resstatus_t status) {
    switch (status) {
        case STATUS_OK: return "OK";              
        case STATUS_CREATED: return "CREATED";         
        case STATUS_ACCEPTED: return "ACCEPTED";        
        case STATUS_NO_CONTENT: return "NO CONTENT";      
        case STATUS_RESET_CONTENT: return "RESET CONTENT";   
        case STATUS_PARTIAL_CONTENT: return "PARTIAL CONTENT"; 
        case STATUS_MULTIPLE_CHOICES: return "MULTIPLE CHOICES";    
        case STATUS_MOVED_PERMANENTLY: return "MOVED PERMANENTLY";   
        case STATUS_MOVED_TEMPORARILY: return "MOVED TEMPORARILY";   
        case STATUS_NOT_MODIFIED: return "NOT MODIFIED";        
        case STATUS_BAD_REQUEST: return "BAD REQUEST";             
        case STATUS_UNAUTHORIZED: return "UNAUTHORIZED";            
        case STATUS_FORBIDDEN: return "FORBIDDEN";               
        case STATUS_NOT_FOUND: return "NOT FOUND";               
        case STATUS_RANGE_NOT_SATISFIABLE: return "RANGE NOT SATISFIABLE";   
        case STATUS_INTERNAL_SERVER_ERROR: return "INTERNAL SERVER_ERROR";   
        case STATUS_NOT_IMPLEMENTED: return "NOT IMPLEMENTED";         
        case STATUS_BAD_GATEWAY: return "BAD GATEWAY";             
        case STATUS_SERVICE_NOT_AVAILABLE: return "SERVICE NOT AVAILABLE";   
        case STATUS_GATEWAY_TIMEOUT: return "GATEWAY TIMEOUT";         
        case STATUS_VERSION_NOT_SUPPORTED: return "VERSION NOT SUPPORTED";   
    }
    return "UNKNOWN";
}

// == HTTP VERSION ============================================================

int httpVerNumber(http_version_t ver) {
    return (ver.major * 10) + ver.minor;
}

// == HTTP REQUEST ============================================================

http_request_t reqInit() {
    http_request_t req = {0};
    reqSetUri(&req, strvInit(""));
    req.version = (http_version_t){1, 1};
    return req;
}

http_request_t reqParse(const char *request) {
    http_request_t req = {0};
    str_istream_t in = istrInit(request);

    // get data

    strview_t method = strvTrim(istrGetview(&in, '/'));
    istrSkip(&in, 1); // skip /
    strview_t page   = strvTrim(istrGetview(&in, ' '));
    strview_t http   = strvTrim(istrGetview(&in, '\n'));

    istrSkip(&in, 1); // skip \n
    
    _parseFields(&req.fields, &in);

    strview_t body = strvTrim(istrGetviewLen(&in, 0, SIZE_MAX));

    // parse data

    // -- method
    const char *methods[] = { "GET", "POST", "HEAD", "PUT", "DELETE" };
    const int methods_count = sizeof(methods) / sizeof(*methods);

    for (int i = 0; i < methods_count; ++i) {
        if (strvCompare(method, strvInit(methods[i])) == 0) {
            req.method = (reqtype_t)i;
        }
    }

    // -- page
    req.uri = strvCopy(page).buf;

    // -- http
    in = istrInitLen(http.buf, http.len);
    istrIgnoreAndSkip(&in, '/'); // skip HTTP/
    istrGetu8(&in, &req.version.major);
    istrSkip(&in, 1); // skip .
    istrGetu8(&in, &req.version.minor);

    // -- body
    req.body = strvCopy(body).buf;

    return req;
}

void reqFree(http_request_t ctx) {
    for (http_field_t *it = ctx.fields; it != vecEnd(ctx.fields); ++it) {
        free(it->key);
        free(it->value);
    }
    vecFree(ctx.fields);
    free(ctx.uri);
    free(ctx.body);
}

bool reqHasField(http_request_t *ctx, const char *key) {
    for(uint32 i = 0; i < vecLen(ctx->fields); ++i) {
        if(stricmp(ctx->fields[i].key, key) == 0) {
            return true;
        }
    }
    return false;
}

void reqSetField(http_request_t *ctx, const char *key, const char *value) {
    _setField(&ctx->fields, key, value);
}

void reqSetUri(http_request_t *ctx, strview_t uri) {
    if (strvIsEmpty(uri)) return;
    free(ctx->uri);
    if (uri.buf[0] == '/') {
        strvRemovePrefix(uri, 1);
    }
    ctx->uri = strvCopy(uri).buf;
}

str_ostream_t reqPrepare(http_request_t *ctx) {
    str_ostream_t out = ostrInitLen(1024);

    const char *method = NULL;
    switch(ctx->method) {
    case REQ_GET:    method = "GET";    break;
    case REQ_POST:   method = "POST";   break;
    case REQ_HEAD:   method = "HEAD";   break;
    case REQ_PUT:    method = "PUT";    break;
    case REQ_DELETE: method = "DELETE"; break;
    default: err("unrecognized method: %d", method); goto error;
    }

    ostrPrintf(&out, "%s /%s HTTP/%hhu.%hhu\r\n", 
        method, ctx->uri, ctx->version.major, ctx->version.minor
    );

    for(uint32 i = 0; i < vecLen(ctx->fields); ++i) {
        ostrPrintf(&out, "%s: %s\r\n", ctx->fields[i].key, ctx->fields[i].value);
    }

    ostrAppendview(&out, strvInit("\r\n"));
    if(ctx->body) {
        ostrAppendview(&out, strvInit(ctx->body));
    }

error:
    return out;
}

str_t reqString(http_request_t *ctx) {
    str_ostream_t out = reqPrepare(ctx);
    return ostrAsStr(out);
}

// == HTTP RESPONSE ===========================================================

http_response_t resParse(const char *data) {
    http_response_t ctx = {0};
    str_istream_t in = istrInit(data);

    char hp[5];
    istrGetstringBuf(&in, hp, 5);
    if(stricmp(hp, "http") != 0) {
        err("response doesn't start with 'HTTP', instead with %c%c%c%c", hp[0], hp[1], hp[2], hp[3]);
        return ctx;
    }
    istrSkip(&in, 1); // skip /
    istrGetu8(&in, &ctx.version.major);
    istrSkip(&in, 1); // skip .
    istrGetu8(&in, &ctx.version.minor);
    istrGeti32(&in, (int32*)&ctx.status_code);

    istrIgnore(&in, '\n');
    istrSkip(&in, 1); // skip \n

    resParseFields(&ctx, &in);

    const char *tran_encoding = resGetField(&ctx, "transfer-encoding");
    if(tran_encoding == NULL || stricmp(tran_encoding, "chunked")  != 0) {
        strview_t body = istrGetviewLen(&in, 0, SIZE_MAX);
        vecClear(ctx.body);
        vecReserve(ctx.body, body.len);
        memcpy(ctx.body, body.buf, body.len);
    }
    else {
        // fatal("chunked encoding not implemented yet");
        err("chunked encoding not implemented yet");
    }

    return ctx;
}   

void resFree(http_response_t ctx) {
    for (http_field_t *it = ctx.fields; it != vecEnd(ctx.fields); ++it) {
        free(it->key);
        free(it->value);
    }
    vecFree(ctx.fields);
    vecFree(ctx.body);
}

bool resHasField(http_response_t *ctx, const char *key) {
    for(uint32 i = 0; i < vecLen(ctx->fields); ++i) {
        if(stricmp(ctx->fields[i].key, key) == 0) {
            return true;
        }
    }
    return false;
}

void resSetField(http_response_t *ctx, const char *key, const char *value) {
    _setField(&ctx->fields, key, value);
}

const char *resGetField(http_response_t *ctx, const char *field) {
    for(uint32 i = 0; i < vecLen(ctx->fields); ++i) {
        if(stricmp(ctx->fields[i].key, field) == 0) {
            return ctx->fields[i].value;
        }
    }
    return NULL;
}

void resParseFields(http_response_t *ctx, str_istream_t *in) {
    _parseFields(&ctx->fields, in);
}

str_ostream_t resPrepare(http_response_t *ctx) {
    str_ostream_t out = ostrInitLen(1024);

    ostrPrintf(
        &out, "HTTP/%hhu.%hhu %d %s\r\n", 
        ctx->version.major, ctx->version.minor, 
        ctx->status_code, httpGetStatusString(ctx->status_code)
    );
    for (http_field_t *field = ctx->fields; field != vecEnd(ctx->fields); ++field) {
        ostrPrintf(&out, "%s: %s\r\n", field->key, field->value);
    }
    ostrPuts(&out, "\r\n");
    ostrAppendview(&out, strvInitLen(ctx->body, vecLen(ctx->body)));

    return out;
}

str_t resString(http_response_t *ctx) {
    str_ostream_t out = resPrepare(ctx);
    return ostrAsStr(out);
}

// == HTTP CLIENT =============================================================

http_client_t hcliInit() {
    return (http_client_t) {
        .port = 80,
    };
}

void hcliFree(http_client_t ctx) {
    strFree(ctx.host_name);
}

void hcliSetHost(http_client_t *ctx, strview_t hostname) {
    // if the hostname starts with http:// (case insensitive)
    if(strvICompare(strvSub(hostname, 0, 7), strvInit("http://")) == 0) {
        ctx->host_name = strvCopy(strvSub(hostname, 7, SIZE_MAX));
    }
    else if(strvICompare(strvSub(hostname, 0, 8), strvInit("https://")) == 0) {
        err("HTTPS protocol not yet supported");
        return;
    }
    else {
        // undefined protocol, use HTTP
        ctx->host_name = strvCopy(hostname);
    }
}

http_response_t hcliSendRequest(http_client_t *ctx, http_request_t *req) {
    if (strBack(ctx->host_name) == '/') {
        ctx->host_name.buf[--ctx->host_name.len] = '\0';
    }
    if(!reqHasField(req, "Host")) {
        reqSetField(req, "Host", ctx->host_name.buf);
    }
    if(!reqHasField(req, "Content-Length")) {
        if(req->body) {
            str_ostream_t out = ostrInitLen(20);
            ostrAppendu64(&out, strlen(req->body));
            reqSetField(req, "Content-Length", out.buf);
            ostrFree(out);
        }
        else {
            reqSetField(req, "Content-Length", "0");
        }
    }
    if(req->method == REQ_POST && !reqHasField(req, "Content-Type")) {
        reqSetField(req, "Content-Type", "application/x-www-form-urlencoded");
    }
    if(httpVerNumber(req->version) >= 11 && !reqHasField(req, "Connection")) {
        reqSetField(req, "Connection", "close");
    }

    http_response_t res = {0};
    str_t req_str = strInit();
    str_ostream_t received = ostrInitLen(1024);

    if(!skInit()) {
        err("couldn't initialize sockets %s", skGetErrorString());
        goto skopen_error;
    }

    ctx->socket = skOpen(SOCK_TCP);
    if(ctx->socket == INVALID_SOCKET) {
        err("couldn't open socket %s", skGetErrorString());
        goto error;
    }

    if(skConnect(ctx->socket, ctx->host_name.buf, ctx->port)) {
        req_str = reqString(req);
        if(req_str.len == 0) {
            err("couldn't get string from request");
            goto error;
        }

        if(skSend(ctx->socket, req_str.buf, (int)req_str.len) == SOCKET_ERROR) {
            err("couldn't send request to socket: %s", skGetErrorString());
            goto error;
        }

        char buffer[1024];
        int read = 0;
        do {
            read = skReceive(ctx->socket, buffer, sizeof(buffer));
            if(read == -1) {
                err("couldn't get the data from the server: %s", skGetErrorString());
                goto error;
            }
            ostrAppendview(&received, strvInitLen(buffer, read));
        } while(read != 0);

        // if the data received is not null terminated
        if(*(received.buf + received.len) != '\0') {
            ostrPutc(&received, '\0');
            received.len--;
        }
        
        res = resParse(received.buf);
    }
    else {
        err("Couldn't connect to host %s -> %s", ctx->host_name, skGetErrorString());
    }

    if(!skClose(ctx->socket)) {
        err("Couldn't close socket");
    }
    
error:
    if(!skCleanup()) {
        err("couldn't clean up sockets %s", skGetErrorString());
    }
skopen_error:
    strFree(req_str);
    ostrFree(received);
    return res;
}

http_response_t httpGet(strview_t hostname, strview_t uri) {
    http_request_t request = reqInit();
    request.method = REQ_GET;
    reqSetUri(&request, uri);

    http_client_t client = hcliInit();
    hcliSetHost(&client, hostname);

    http_response_t res = hcliSendRequest(&client, &request);

    reqFree(request);
    hcliFree(client);

    return res;
}

url_split_t urlSplit(strview_t uri) {
    url_split_t out = {0};

    if (strvStartsWithView(uri, strvInit("https://"))) {
        uri = strvRemovePrefix(uri, 8);
    }
    else if (strvStartsWithView(uri, strvInit("http://"))) {
        uri = strvRemovePrefix(uri, 7);
    }

    out.host = strvSub(uri, 0, strvFind(uri, '/', 0));
    out.uri = strvSub(uri, out.host.len, SIZE_MAX);
    return out;
}
