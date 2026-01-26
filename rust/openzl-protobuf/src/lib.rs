use std::ffi::{CStr, CString};
use std::fmt;
use std::os::raw::c_char;
use std::ptr::NonNull;

#[repr(C)]
struct OpenZLProtobufContext {
    _private: [u8; 0],
}

#[repr(C)]
#[derive(Debug, Copy, Clone)]
enum OpenZLProtobufSchemaType {
    OpenZlProtobufSchemaProto = 0,
    OpenZlProtobufSchemaDescriptor = 1,
}

#[repr(C)]
#[derive(Debug, Copy, Clone)]
enum OpenZLProtobufClusteringTrainer {
    OpenZlProtobufClusteringTrainerGreedy = 0,
    OpenZlProtobufClusteringTrainerBottomUp = 1,
    OpenZlProtobufClusteringTrainerFullSplit = 2,
}

#[repr(C)]
struct OpenZLProtobufSchema {
    schema_type: OpenZLProtobufSchemaType,
    schema_path: *const c_char,
    message_type: *const c_char,
    proto_paths: *const *const c_char,
    proto_paths_len: usize,
}

#[repr(C)]
#[derive(Debug, Copy, Clone)]
struct OpenZLBuffer {
    data: *mut u8,
    len: usize,
}

#[repr(C)]
#[derive(Debug, Copy, Clone)]
struct OpenZLProtobufTrainParams {
    has_threads: u8,
    threads: u32,
    has_clustering_trainer: u8,
    clustering_trainer: OpenZLProtobufClusteringTrainer,
    has_max_time_secs: u8,
    max_time_secs: usize,
    has_no_ace_successors: u8,
    no_ace_successors: u8,
    has_no_clustering: u8,
    no_clustering: u8,
}

extern "C" {
    fn openzl_protobuf_create(schema: *const OpenZLProtobufSchema)
        -> *mut OpenZLProtobufContext;
    fn openzl_protobuf_destroy(ctx: *mut OpenZLProtobufContext);
    fn openzl_protobuf_last_error(ctx: *const OpenZLProtobufContext) -> *const c_char;
    fn openzl_protobuf_set_compressor(
        ctx: *mut OpenZLProtobufContext,
        compressor_data: *const u8,
        compressor_len: usize,
    ) -> i32;
    fn openzl_protobuf_compress(
        ctx: *mut OpenZLProtobufContext,
        proto_data: *const u8,
        proto_len: usize,
        out: *mut OpenZLBuffer,
    ) -> i32;
    fn openzl_protobuf_decompress(
        ctx: *mut OpenZLProtobufContext,
        zl_data: *const u8,
        zl_len: usize,
        out: *mut OpenZLBuffer,
    ) -> i32;
    fn openzl_protobuf_train(
        ctx: *mut OpenZLProtobufContext,
        samples: *const OpenZLBuffer,
        samples_len: usize,
        out_compressor: *mut OpenZLBuffer,
    ) -> i32;
    fn openzl_protobuf_train_with_params(
        ctx: *mut OpenZLProtobufContext,
        samples: *const OpenZLBuffer,
        samples_len: usize,
        params: *const OpenZLProtobufTrainParams,
        out_compressor: *mut OpenZLBuffer,
    ) -> i32;
    fn openzl_protobuf_free_buffer(buffer: *mut OpenZLBuffer);
}

/// Protobuf schema descriptor for OpenZL bindings.
pub enum Schema {
    /// Load schema from a .proto file and optional include paths.
    ///
    /// `path` is resolved via the `proto_paths` roots (like `protoc --proto_path`),
    /// so it should be relative to one of those directories rather than an
    /// absolute filesystem path.
    Proto {
        path: String,
        message_type: String,
        proto_paths: Vec<String>,
    },
    /// Load schema from a compiled descriptor (.desc) file.
    ///
    /// `path` is passed directly to the filesystem, so it can be absolute or
    /// relative to the current working directory.
    Descriptor { path: String, message_type: String },
}

/// Training algorithm selection for clustering graphs.
#[derive(Debug, Copy, Clone)]
pub enum ClusteringTrainer {
    Greedy,
    BottomUp,
    FullSplit,
}

/// Training parameters for clustering-based compressors.
#[derive(Debug, Copy, Clone)]
pub struct TrainParams {
    pub threads: Option<u32>,
    pub clustering_trainer: Option<ClusteringTrainer>,
    pub max_time_secs: Option<usize>,
    pub no_ace_successors: bool,
    pub no_clustering: bool,
}

impl Default for TrainParams {
    fn default() -> Self {
        Self {
            threads: None,
            clustering_trainer: None,
            max_time_secs: None,
            no_ace_successors: true,
            no_clustering: false,
        }
    }
}

impl Schema {
    /// Create a schema that loads from a .proto file.
    pub fn proto(
        path: impl Into<String>,
        message_type: impl Into<String>,
        proto_paths: Vec<String>,
    ) -> Self {
        Self::Proto {
            path: path.into(),
            message_type: message_type.into(),
            proto_paths,
        }
    }

    /// Create a schema that loads from a descriptor set file.
    pub fn descriptor(path: impl Into<String>, message_type: impl Into<String>) -> Self {
        Self::Descriptor {
            path: path.into(),
            message_type: message_type.into(),
        }
    }
}

/// Errors returned by the OpenZL protobuf bindings.
#[derive(Debug)]
pub struct Error {
    message: String,
}

impl Error {
    fn new(message: impl Into<String>) -> Self {
        Self {
            message: message.into(),
        }
    }
}

impl fmt::Display for Error {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "{}", self.message)
    }
}

impl std::error::Error for Error {}

/// OpenZL protobuf compressor handle.
pub struct OpenZLProtobuf {
    ctx: NonNull<OpenZLProtobufContext>,
}

impl OpenZLProtobuf {
    /// Create a new OpenZL protobuf compressor.
    pub fn new(schema: Schema) -> Result<Self, Error> {
        let (schema_type, schema_path, message_type, proto_paths) = match schema {
            Schema::Proto {
                path,
                message_type,
                proto_paths,
            } => (
                OpenZLProtobufSchemaType::OpenZlProtobufSchemaProto,
                path,
                message_type,
                proto_paths,
            ),
            Schema::Descriptor { path, message_type } => (
                OpenZLProtobufSchemaType::OpenZlProtobufSchemaDescriptor,
                path,
                message_type,
                Vec::new(),
            ),
        };

        let schema_path = CString::new(schema_path)
            .map_err(|_| Error::new("Schema path contains a null byte."))?;
        let message_type = CString::new(message_type)
            .map_err(|_| Error::new("Message type contains a null byte."))?;

        let mut proto_path_storage = Vec::new();
        let mut proto_path_ptrs = Vec::new();
        for path in proto_paths {
            let c_path = CString::new(path)
                .map_err(|_| Error::new("Proto path contains a null byte."))?;
            proto_path_ptrs.push(c_path.as_ptr());
            proto_path_storage.push(c_path);
        }

        let schema = OpenZLProtobufSchema {
            schema_type,
            schema_path: schema_path.as_ptr(),
            message_type: message_type.as_ptr(),
            proto_paths: proto_path_ptrs.as_ptr(),
            proto_paths_len: proto_path_ptrs.len(),
        };

        let ctx = unsafe { openzl_protobuf_create(&schema) };
        let ctx = NonNull::new(ctx)
            .ok_or_else(|| Error::new("Failed to create OpenZL protobuf context."))?;

        let last_error = unsafe {
            let ptr = openzl_protobuf_last_error(ctx.as_ptr());
            if ptr.is_null() {
                "Unknown OpenZL error.".to_string()
            } else {
                CStr::from_ptr(ptr).to_string_lossy().into_owned()
            }
        };
        if !last_error.is_empty() {
            unsafe { openzl_protobuf_destroy(ctx.as_ptr()) };
            return Err(Error::new(last_error));
        }

        Ok(Self { ctx })
    }

    /// Load a serialized compressor into the OpenZL encoder.
    pub fn set_compressor(&mut self, compressor: &[u8]) -> Result<(), Error> {
        let ok = unsafe {
            openzl_protobuf_set_compressor(self.ctx.as_ptr(), compressor.as_ptr(), compressor.len())
        };
        if ok == 1 {
            Ok(())
        } else {
            Err(self.error_from_last("Failed to set compressor."))
        }
    }

    /// Compress a protobuf message encoded in standard wire format.
    pub fn compress(&self, proto_bytes: &[u8]) -> Result<Vec<u8>, Error> {
        let mut out = OpenZLBuffer {
            data: std::ptr::null_mut(),
            len: 0,
        };
        let ok = unsafe {
            openzl_protobuf_compress(
                self.ctx.as_ptr(),
                proto_bytes.as_ptr(),
                proto_bytes.len(),
                &mut out,
            )
        };
        if ok != 1 {
            return Err(self.error_from_last("Compression failed."));
        }
        Ok(unsafe { take_buffer(out) })
    }

    /// Decompress OpenZL protobuf data into standard wire format.
    pub fn decompress(&self, zl_bytes: &[u8]) -> Result<Vec<u8>, Error> {
        let mut out = OpenZLBuffer {
            data: std::ptr::null_mut(),
            len: 0,
        };
        let ok = unsafe {
            openzl_protobuf_decompress(
                self.ctx.as_ptr(),
                zl_bytes.as_ptr(),
                zl_bytes.len(),
                &mut out,
            )
        };
        if ok != 1 {
            return Err(self.error_from_last("Decompression failed."));
        }
        Ok(unsafe { take_buffer(out) })
    }

    /// Train a compressor from a set of protobuf messages.
    pub fn train_compressor(&self, samples: &[&[u8]]) -> Result<Vec<u8>, Error> {
        if samples.is_empty() {
            return Err(Error::new("Training samples are empty."));
        }

        let buffers: Vec<OpenZLBuffer> = samples
            .iter()
            .map(|sample| OpenZLBuffer {
                data: sample.as_ptr() as *mut u8,
                len: sample.len(),
            })
            .collect();

        let mut out = OpenZLBuffer {
            data: std::ptr::null_mut(),
            len: 0,
        };
        let ok = unsafe {
            openzl_protobuf_train(
                self.ctx.as_ptr(),
                buffers.as_ptr(),
                buffers.len(),
                &mut out,
            )
        };
        if ok != 1 {
            return Err(self.error_from_last("Training failed."));
        }
        Ok(unsafe { take_buffer(out) })
    }

    /// Train a compressor from a set of protobuf messages with parameters.
    pub fn train_compressor_with_params(
        &self,
        samples: &[&[u8]],
        params: TrainParams,
    ) -> Result<Vec<u8>, Error> {
        if samples.is_empty() {
            return Err(Error::new("Training samples are empty."));
        }

        let buffers: Vec<OpenZLBuffer> = samples
            .iter()
            .map(|sample| OpenZLBuffer {
                data: sample.as_ptr() as *mut u8,
                len: sample.len(),
            })
            .collect();

        let ffi_params = OpenZLProtobufTrainParams {
            has_threads: params.threads.is_some() as u8,
            threads: params.threads.unwrap_or_default(),
            has_clustering_trainer: params.clustering_trainer.is_some() as u8,
            clustering_trainer: match params.clustering_trainer.unwrap_or(ClusteringTrainer::Greedy)
            {
                ClusteringTrainer::Greedy => {
                    OpenZLProtobufClusteringTrainer::OpenZlProtobufClusteringTrainerGreedy
                }
                ClusteringTrainer::BottomUp => {
                    OpenZLProtobufClusteringTrainer::OpenZlProtobufClusteringTrainerBottomUp
                }
                ClusteringTrainer::FullSplit => {
                    OpenZLProtobufClusteringTrainer::OpenZlProtobufClusteringTrainerFullSplit
                }
            },
            has_max_time_secs: params.max_time_secs.is_some() as u8,
            max_time_secs: params.max_time_secs.unwrap_or_default(),
            has_no_ace_successors: 1,
            no_ace_successors: params.no_ace_successors as u8,
            has_no_clustering: 1,
            no_clustering: params.no_clustering as u8,
        };

        let mut out = OpenZLBuffer {
            data: std::ptr::null_mut(),
            len: 0,
        };
        let ok = unsafe {
            openzl_protobuf_train_with_params(
                self.ctx.as_ptr(),
                buffers.as_ptr(),
                buffers.len(),
                &ffi_params,
                &mut out,
            )
        };
        if ok != 1 {
            return Err(self.error_from_last("Training failed."));
        }
        Ok(unsafe { take_buffer(out) })
    }

    fn last_error(&self) -> String {
        let ptr = unsafe { openzl_protobuf_last_error(self.ctx.as_ptr()) };
        if ptr.is_null() {
            return "Unknown OpenZL error.".to_string();
        }
        unsafe { CStr::from_ptr(ptr).to_string_lossy().into_owned() }
    }

    fn error_from_last(&self, fallback: &str) -> Error {
        let msg = self.last_error();
        if msg.is_empty() {
            Error::new(fallback)
        } else {
            Error::new(msg)
        }
    }
}

impl Drop for OpenZLProtobuf {
    fn drop(&mut self) {
        unsafe { openzl_protobuf_destroy(self.ctx.as_ptr()) };
    }
}

unsafe fn take_buffer(mut buffer: OpenZLBuffer) -> Vec<u8> {
    if buffer.len == 0 || buffer.data.is_null() {
        openzl_protobuf_free_buffer(&mut buffer);
        return Vec::new();
    }
    let slice = std::slice::from_raw_parts(buffer.data, buffer.len);
    let data = slice.to_vec();
    openzl_protobuf_free_buffer(&mut buffer);
    data
}
