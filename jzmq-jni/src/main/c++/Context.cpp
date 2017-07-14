/*
    Copyright (c) 2007-2013 Contributors as noted in the AUTHORS file

    This file is part of 0MQ.

    0MQ is free software; you can redistribute it and/or modify it under
    the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation; either version 3 of the License, or
    (at your option) any later version.

    0MQ is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <assert.h>

#include <zmq.h>
#include <jni_md.h>

#include "jzmq.hpp"
#include "util.hpp"
#include "org_zeromq_ZMQ_Context.h"

static jfieldID contextptrFID;

static void ensure_context (JNIEnv *env, jobject obj);
static void *get_context (JNIEnv *env, jobject obj);
static void put_context (JNIEnv *env, jobject obj, void *s);


/**
 * Called to construct a Java Context object.
 */
JNIEXPORT void JNICALL
Java_org_zeromq_ZMQ_00024Context_construct (JNIEnv *env, jobject obj, jint io_threads)
{
    void *c = get_context (env, obj);
    if (c)
        return;

    c = zmq_init (io_threads);
    int err = zmq_errno();
    put_context (env, obj, c);

    if (c == NULL) {
        raise_exception (env, err);
        return;
    }
}

/**
 * Called to destroy a Java Context object.
 */
JNIEXPORT void JNICALL
Java_org_zeromq_ZMQ_00024Context_destroy (JNIEnv *env, jobject obj) {
    void *c = get_context (env, obj);
    if (! c)
        return;

    int rc = zmq_term (c);
    int err = zmq_errno();
    c = NULL;
    put_context (env, obj, c);

    if (rc != 0) {
        raise_exception (env, err);
        return;
    }
}

struct ctx_data
{
	JavaVM* vm;
	jobject obj;
	jmethodID method;

	jclass errClass;
	jmethodID errMethod;
};

jobject errorByCode(JNIEnv* env, jclass errClass, jmethodID errMethod, int err)
{
	if(env==NULL || errClass==NULL || errMethod==NULL)
		return NULL;

	return env->CallStaticObjectMethod(errClass, errMethod, err);
}

void zmq_error_cb(int err, const char* host, void* data)
{
	ctx_data* ctx = static_cast<ctx_data*>(data);
	if(!ctx) {
		return;
	}

	JNIEnv* env;
	int getEnvStat = ctx->vm->GetEnv((void **)&env, JNI_VERSION_1_8);

	if (getEnvStat == JNI_EDETACHED) {		
		if (ctx->vm->AttachCurrentThread((void **)&env, NULL) != 0) {
			printf("JNI Error: Failed to attach to current thread!\n");
			return;
		}
	}
	else if (getEnvStat == JNI_OK) {
		//
	}
	else if (getEnvStat == JNI_EVERSION) {
		printf("JNI Error: Unsupported JVM version!\n");
		return;
	}

	jstring str = env->NewStringUTF(host);
	env->CallVoidMethod(ctx->obj, ctx->method, errorByCode(env, ctx->errClass, ctx->errMethod, err), str);
	ctx->vm->DetachCurrentThread();
}

JNIEXPORT jboolean JNICALL Java_org_zeromq_ZMQ_00024Context_setErrorHandler
(JNIEnv *env, jobject obj, jobject error)
{
#if ZMQ_VERSION >= ZMQ_MAKE_VERSION(4,2,3)
	void *c = get_context(env, obj);
	if (!c)
		return JNI_FALSE;
	
	int result = 0;

	if(error==NULL)
	{
		result = zmq_error_handler(c, NULL, NULL);
	}
	else {
		//env->NewGlobalRef(error);
		jstring str = env->NewStringUTF("test string");		

		jclass objclass = env->GetObjectClass(error);
		jmethodID method = env->GetMethodID(objclass, "reportError", "(Lorg/zeromq/ZMQ$Error;Ljava/lang/String;)I");
		if(method==NULL)
		{
			return JNI_FALSE;
		}

		ctx_data* data = static_cast<ctx_data*>(malloc(sizeof(ctx_data)));
		if(!data)
		{
			return JNI_FALSE;
		}

		data->method = method;
		data->obj = env->NewGlobalRef(error);
		data->vm = NULL;
		env->GetJavaVM(&data->vm);

		data->errClass = env->FindClass("org/zeromq/ZMQ$Error");
		if (!data->errClass)
		{
			printf("JNI Error: org/zeromq/ZMQ$Error class not found!\n");
		}
		else {
			data->errMethod = env->GetStaticMethodID(data->errClass, "findByCode", "(I)Lorg/zeromq/ZMQ$Error;");
			if (!data->errMethod)
			{
				printf("JNI Error: org/zeromq/ZMQ$Error.findByCode(int) method not found!\n");
			}
		}

		result = zmq_error_handler(c, zmq_error_cb, data);
	}
	return result == 0;
#else
	return JNI_FALSE;
#endif
}

JNIEXPORT jboolean JNICALL
Java_org_zeromq_ZMQ_00024Context_setMaxSockets (JNIEnv * env, jobject obj, jint maxSockets)
{
#if ZMQ_VERSION >= ZMQ_MAKE_VERSION(3,0,0)
    void *c = get_context (env, obj);
    if (! c)
        return JNI_FALSE;
    int result = zmq_ctx_set (c, ZMQ_MAX_SOCKETS, maxSockets);
    return result == 0;
#else
    return JNI_FALSE;
#endif
}

JNIEXPORT jint JNICALL
Java_org_zeromq_ZMQ_00024Context_getMaxSockets (JNIEnv *env, jobject obj)
{
#if ZMQ_VERSION >= ZMQ_MAKE_VERSION(3,0,0)
    void *c = get_context (env, obj);
    if (! c)
        return -1;

    return zmq_ctx_get (c, ZMQ_MAX_SOCKETS);
#else
    return -1;
#endif
}

/**
 * Make sure we have a valid pointer to Java's Context::contextHandle.
 */
static void ensure_context (JNIEnv *env, jobject obj)
{
    if (contextptrFID == NULL) {
        jclass cls = env->GetObjectClass (obj);
        assert (cls);
        contextptrFID = env->GetFieldID (cls, "contextHandle", "J");
        assert (contextptrFID);
        env->DeleteLocalRef (cls);
    }
}

/**
 * Get the value of Java's Context::contextHandle.
 */
static void *get_context (JNIEnv *env, jobject obj)
{
    ensure_context (env, obj);
    void *s = (void*) env->GetLongField (obj, contextptrFID);

    return s;
}

/**
 * Set the value of Java's Context::contextHandle.
 */
static void put_context (JNIEnv *env, jobject obj, void *s)
{
    ensure_context (env, obj);
    env->SetLongField (obj, contextptrFID, (jlong) s);
}
