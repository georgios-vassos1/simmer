# Copyright (C) 2015 Iñaki Ucar and Bart Smeets
# Copyright (C) 2015-2019 Iñaki Ucar
#
# This file is part of simmer.
#
# simmer is free software: you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 2 of the License, or
# (at your option) any later version.
#
# simmer is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with simmer. If not, see <http://www.gnu.org/licenses/>.

#.onUnload <- function (libpath) {
#  library.dynam.unload("simmer", libpath)
#}

is_flag <- function(name, env)
  is.numeric(env[[name]]) || is.logical(env[[name]])

is_NA <- function(name, env) is.na(env[[name]])

is_numeric <- function(name, env) is.numeric(env[[name]])

is_function <- function(name, env) {
  if (!is.function(env[[name]])) return(FALSE)
  env[[name]] <- magrittr_workaround(env[[name]])
  TRUE
}

is_trajectory <- function(name, env) {
  if (name == "dots.")
    all(sapply(env[[name]], inherits, what="trajectory"))
  else inherits(env[[name]], "trajectory")
}

get_caller <- function(n=1) {
  sub("\\.[[:alpha:]]+$", "", as.character(sys.call(-n-1))[[1]])
}

check_args <- function(..., env.=parent.frame()) {
  types <- list(...)
  msg <- NULL
  ns <- getNamespace("simmer")

  for (var in names(types)) {
    check <- sapply(types[[var]], function(type) {
      func <- paste0("is_", sub(" ", "_", type))
      if (exists(func, ns, inherits=FALSE))
        return(do.call(ns[[func]], list(var, env.)))
      inherits(env.[[var]], type)
    })
    if (!any(check)) msg <- c(msg, paste0(
      "'", sub("dots.", "...", var), "' is not a valid ", paste0(types[[var]], collapse=" or ")))
  }

  if (length(msg))
    stop(get_caller(2), ": ", paste0(msg, collapse=", "), call.=FALSE)
}

positive <- function(x) {
  x <- abs(x)
  x[is.infinite(x)] <- -1
  x
}

envs_apply <- function(envs, method, ...) {
  if (!is.list(envs)) envs <- list(envs)
  args <- list(...)

  do.call(rbind, lapply(1:length(envs), function(i) {
    stats <- do.call(method, c(envs[[i]], args))
    if (nrow(stats)) stats$replication <- i
    else cbind(stats, data.frame(replication = character()))
    stats
  }))
}

#' @importFrom codetools findGlobals
make_resetable <- function(func) {
  # find globals and get init values
  init <- sapply(findGlobals(func, merge=FALSE)$variables,
                 get0, envir=environment(func), simplify=FALSE)
  # avoid simulator overwrite in some circumstances
  init <- init[!sapply(init, function(x) is.null(x) | inherits(x, "simmer"))]

  # attach reset attribute
  env <- list2env(list(init=init, env=environment(func)))
  attr(func, "reset") <- function() {
    for (i in ls(init, all.names = TRUE))
      assign(i, init[[i]], env, inherits=TRUE)
  }
  environment(attr(func, "reset")) <- env
  func
}

getval <- function(x) if (is.function(x)) x() else x

replace_env <- function(..., envir=parent.frame()) {
  for (obj in list(...)) {
    if (!is.function(obj)) next
    for (var in ls(environment(obj)))
      assign(var, get(var, environment(obj)), envir)
    environment(obj) <- envir
  }
}

binarise <- function(...) {
  args <- c(...)
  sum(2^(seq_along(args) - 1) * args) + 1
}

# see https://github.com/tidyverse/magrittr/issues/146
magrittr_workaround <- function(func) {
  if (!identical(environment(func), .GlobalEnv) &&
      "." %in% ls(envir=environment(func), all.names=TRUE))
    rm(".", envir=environment(func))
  func
}

recycle <- function(param, n) {
  if (length(param) != 1 || n == 1)
    return(param)
  rep(param, n)
}
