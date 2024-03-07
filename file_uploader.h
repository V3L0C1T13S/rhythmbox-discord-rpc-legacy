#pragma once

#include "rb-debug.h"
#include "rb-discord.h"

#include "include/c-hashmap/map.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef char ArtURL;
#define ART_URL_SIZE 1000

typedef struct _ArtWorkerCallbackParams {
  RBDiscordPlugin *self;
  RhythmDBEntry *entry;
  ArtURL file_url[ART_URL_SIZE];
} ArtworkerCallbackParams;
typedef void (*ArtWorkerCallback)(ArtworkerCallbackParams *params);

typedef struct _ArtWorkerUploadParams {
  RBDiscordPlugin *self;
  ArtWorkerCallback cb;
  const char *art_path;
  RhythmDBEntry *entry;
} ArtWorkerUploadParams;

hashmap *m;

void art_upload_worker(ArtWorkerUploadParams *params) {
  FILE *fp;
  const char *art_path = params->art_path;
  ArtworkerCallbackParams cb_params;
  uintptr_t cached_art;

  cb_params.entry = params->entry;
  cb_params.self = params->self;

  if (hashmap_get(m, art_path, strlen(art_path), &cached_art)) {
    rb_debug("Using already uploaded art for %s - %s", art_path,
             (char *)cached_art);
    strcat(cb_params.file_url, (char *)cached_art);
    params->cb(&cb_params);

    return;
  }

  const char *python_bin_path = "/bin/python3";
  // TODO: unhardcode this pls and thank you
  const char *python_file_path =
      " /usr/lib/rhythmbox/plugins/rhythmbox-discord/uploader.py ";
  char command_line[strlen(python_bin_path) + strlen(python_file_path) +
                    strlen(art_path)];

  strcpy(command_line, python_bin_path);
  strcat(command_line, python_file_path);
  strcat(command_line, art_path);

  rb_debug("starting python with cmd %s\n", command_line);

  fp = popen(command_line, "r");

  if (fp == NULL) {
    rb_debug("Failed to run command\n");
    return;
  }

  /* Read the output a line at a time - output it. */
  fscanf(fp, "%s", cb_params.file_url);
  pclose(fp);

  rb_debug("uploaded file! %s", cb_params.file_url);

  const char *key_copy = malloc(strlen(art_path));
  strcpy(key_copy, art_path);

  hashmap_set(m, key_copy, strlen(key_copy), (uintptr_t)cb_params.file_url);

  params->cb(&cb_params);

  free(params);
}

void upload_art(RBDiscordPlugin *self, const char *art_path,
                RhythmDBEntry *entry, ArtWorkerCallback cb) {
  if (m == NULL)
    m = hashmap_create();

  pthread_t worker_thid;
  ArtWorkerUploadParams params;

  params.cb = cb;
  params.self = self;
  params.art_path = art_path;
  params.entry = rhythmdb_entry_ref(entry);

  // Allocate memory for the copy on the heap
  ArtWorkerUploadParams *params_copy = malloc(sizeof(ArtWorkerUploadParams));

  // Copy the contents of params to params_copy
  memcpy(params_copy, &params, sizeof(ArtWorkerUploadParams));

  pthread_create(&worker_thid, NULL, art_upload_worker, params_copy);
}