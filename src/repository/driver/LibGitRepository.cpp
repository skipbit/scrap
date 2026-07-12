#include "repository/driver/LibGitRepository.h"

#include <git2.h>
#include <git2/annotated_commit.h>
#include <git2/merge.h>
#include <system_error>

enum class GitError {
    RemoteLookupFailed = 1,
    FetchFailed,
    CommitLookupFailed,
    ReferenceLookupFailed,
    AnnotatedCommitCreationFailed,
    CloneOptionsFailed,
    CloneFailed,
    RepositoryOpenFailed,
    MergeAnalysisFailed,
    MergeOptionsFailed,
    MergeFailed,
    CheckoutOptionsFailed,
    CheckoutFailed,
    SetHeadFailed,
    SetReferenceTargetFailed
};

namespace std {
template <> struct is_error_code_enum<GitError> : true_type { };
}  // namespace std

class GitErrorCategory : public std::error_category {
public:
    const char* name() const noexcept override
    {
        return "git";
    }

    std::string message(int ev) const override
    {
        switch (static_cast<GitError>(ev)) {
            case GitError::RemoteLookupFailed:
                return "Failed to lookup remote";
            case GitError::FetchFailed:
                return "Failed to fetch remote";
            case GitError::CommitLookupFailed:
                return "Failed to lookup commit";
            case GitError::ReferenceLookupFailed:
                return "Failed to lookup reference";
            case GitError::AnnotatedCommitCreationFailed:
                return "Failed to create annotated commit";
            case GitError::CloneOptionsFailed:
                return "Failed to initialize clone options";
            case GitError::CloneFailed:
                return "Failed to clone repository";
            case GitError::RepositoryOpenFailed:
                return "Failed to open repository";
            case GitError::MergeAnalysisFailed:
                return "Failed to analyze merge";
            case GitError::MergeOptionsFailed:
                return "Failed to initialize merge options";
            case GitError::MergeFailed:
                return "Failed to merge";
            case GitError::CheckoutOptionsFailed:
                return "Failed to initialize checkout options";
            case GitError::CheckoutFailed:
                return "Failed to checkout head";
            case GitError::SetHeadFailed:
                return "Failed to set repository head";
            case GitError::SetReferenceTargetFailed:
                return "Failed to set reference target";
            default:
                return "Unknown git error";
        }
    }
};

const GitErrorCategory& gitErrorCategory()
{
    static GitErrorCategory instance;
    return instance;
}

std::error_code make_error_code(GitError e)
{
    return {static_cast<int>(e), gitErrorCategory()};
}

namespace scrap::repository::libgit {

/**
 * @brief RAII wrapper for git_commit
 */
class Commit {
public:
    explicit Commit(git_commit* commit)
        : commit_(commit)
    {
    }

    ~Commit()
    {
        if (commit_) {
            git_commit_free(commit_);
        }
    }

    Commit(const Commit&) = delete;
    Commit& operator=(const Commit&) = delete;

    Commit(Commit&& other) noexcept
        : commit_(other.commit_)
    {
        other.commit_ = nullptr;
    }

    Commit& operator=(Commit&& other) noexcept
    {
        if (this != &other) {
            if (commit_) {
                git_commit_free(commit_);
            }
            commit_ = other.commit_;
            other.commit_ = nullptr;
        }
        return *this;
    }

    const git_oid* oid() const
    {
        return git_commit_id(commit_);
    }

    git_commit* get() const
    {
        return commit_;
    }

private:
    git_commit* commit_ = nullptr;
};

/**
 * @brief RAII wrapper for git_reference
 */
class Reference {
public:
    explicit Reference(git_reference* reference)
        : reference_(reference)
    {
    }

    ~Reference()
    {
        if (reference_) {
            git_reference_free(reference_);
        }
    }

    Reference(const Reference&) = delete;
    Reference& operator=(const Reference&) = delete;

    Reference(Reference&& other) noexcept
        : reference_(other.reference_)
    {
        other.reference_ = nullptr;
    }

    Reference& operator=(Reference&& other) noexcept
    {
        if (this != &other) {
            if (reference_) {
                git_reference_free(reference_);
            }
            reference_ = other.reference_;
            other.reference_ = nullptr;
        }
        return *this;
    }

    const git_oid* oid() const
    {
        return git_reference_target(reference_);
    }

    const char* name() const
    {
        return git_reference_name(reference_);
    }

    std::expected<void, std::error_code> setTarget(const Commit& c, const std::string& message)
    {
        if (git_reference_set_target(&reference_, reference_, c.oid(), message.c_str()) != GIT_OK) {
            return std::unexpected(make_error_code(GitError::SetReferenceTargetFailed));
        }
        return {};
    }

    git_reference* get() const
    {
        return reference_;
    }

private:
    git_reference* reference_ = nullptr;
};

/**
 * @brief The internal class for the repository class.
 */
class Repository::Internal {
public:
    // --
    Internal(const std::filesystem::path& path)
    {
        if (git_repository_open(&repository, path.c_str()) != GIT_OK) {
            repository = nullptr;  // Ensure it's null on failure
        }
    }
    // --
    Internal(const std::string& url, const std::filesystem::path& path)
    {
        git_clone_options options;
        if (git_clone_options_init(&options, GIT_CLONE_OPTIONS_VERSION) != GIT_OK) {
            repository = nullptr;
            return;
        }

        if (git_clone(&repository, url.c_str(), path.c_str(), &options) != GIT_OK) {
            repository = nullptr;
        }
    }

    bool isValid() const
    {
        return repository != nullptr;
    }

    std::error_code lastError() const
    {
        // For now, return a generic error. In a more sophisticated implementation,
        // we could capture the specific libgit2 error
        return make_error_code(repository ? GitError::RemoteLookupFailed : GitError::RepositoryOpenFailed);
    }
    // --
    ~Internal()
    {
        if (repository) {
            git_repository_free(repository);
        }
    }

    // --
    std::expected<void, std::error_code> update(const std::string& r = "origin", const std::string& b = "main")
    {
        auto fetchResult = fetch(r);
        if (! fetchResult) {
            return std::unexpected(fetchResult.error());
        }

        auto refResult = createReference("refs/remotes/" + r + "/" + b);
        if (! refResult) {
            return std::unexpected(refResult.error());
        }

        auto commitResult = createCommit(refResult->oid());
        if (! commitResult) {
            return std::unexpected(commitResult.error());
        }

        auto localRefResult = createReference("refs/heads/" + b);
        if (! localRefResult) {
            return std::unexpected(localRefResult.error());
        }

        return merge(*commitResult, *localRefResult);
    }

    // --
    std::expected<void, std::error_code> fetch(const std::string& r = "origin")
    {
        git_remote* remote = nullptr;
        if (git_remote_lookup(&remote, repository, r.c_str()) != GIT_OK) {
            return std::unexpected(make_error_code(GitError::RemoteLookupFailed));
        }

        auto cleanup = [remote]() {
            git_remote_free(remote);
        };

        if (git_remote_fetch(remote, nullptr, nullptr, nullptr) != GIT_OK) {
            cleanup();
            return std::unexpected(make_error_code(GitError::FetchFailed));
        }

        cleanup();
        return {};
    }

    std::expected<Reference, std::error_code> createReference(const std::string& name)
    {
        git_reference* reference = nullptr;
        if (git_reference_lookup(&reference, repository, name.c_str()) != GIT_OK) {
            return std::unexpected(make_error_code(GitError::ReferenceLookupFailed));
        }
        return Reference{reference};
    }

    std::expected<Commit, std::error_code> createCommit(const git_oid* oid)
    {
        git_commit* commit = nullptr;
        if (git_commit_lookup(&commit, repository, oid) != GIT_OK) {
            return std::unexpected(make_error_code(GitError::CommitLookupFailed));
        }
        return Commit{commit};
    }

    // --
    std::expected<void, std::error_code> merge(Commit& co, Reference& ref)
    {
        git_merge_analysis_t analysis;
        git_merge_preference_t preference;

        git_annotated_commit* annotation = nullptr;
        if (git_annotated_commit_from_ref(&annotation, repository, ref.get()) != GIT_OK) {
            return std::unexpected(make_error_code(GitError::AnnotatedCommitCreationFailed));
        }

        auto cleanup = [annotation]() {
            git_annotated_commit_free(annotation);
        };
        const git_annotated_commit* annotations[] = {annotation};

        if (git_merge_analysis(&analysis, &preference, repository, annotations, 1) != GIT_OK) {
            cleanup();
            return std::unexpected(make_error_code(GitError::MergeAnalysisFailed));
        }

        if (analysis & GIT_MERGE_ANALYSIS_UP_TO_DATE) {
            cleanup();
            return {};
        } else if (analysis & GIT_MERGE_ANALYSIS_FASTFORWARD) {
            auto setTargetResult = ref.setTarget(co, "Fast-forward");
            if (! setTargetResult) {
                cleanup();
                return std::unexpected(setTargetResult.error());
            }

            auto setHeadResult = setHeadToRef(ref);
            if (! setHeadResult) {
                cleanup();
                return std::unexpected(setHeadResult.error());
            }

            auto checkoutResult = checkoutHead();
            cleanup();
            return checkoutResult;
        } else if (analysis & GIT_MERGE_ANALYSIS_NORMAL) {
            git_merge_options options;
            if (git_merge_init_options(&options, GIT_MERGE_OPTIONS_VERSION) != GIT_OK) {
                cleanup();
                return std::unexpected(make_error_code(GitError::MergeOptionsFailed));
            }

            if (git_merge(repository, annotations, 1, &options, nullptr) != GIT_OK) {
                cleanup();
                return std::unexpected(make_error_code(GitError::MergeFailed));
            }
        }

        cleanup();
        return {};
    }

    // --
    std::expected<void, std::error_code> setHeadToRef(const Reference& l)
    {
        if (git_repository_set_head(repository, l.name()) != GIT_OK) {
            return std::unexpected(make_error_code(GitError::SetHeadFailed));
        }
        return {};
    }

    // --
    std::expected<void, std::error_code> checkoutHead()
    {
        git_checkout_options options;
        if (git_checkout_options_init(&options, GIT_CHECKOUT_OPTIONS_VERSION) != GIT_OK) {
            return std::unexpected(make_error_code(GitError::CheckoutOptionsFailed));
        }

        if (git_checkout_head(repository, &options) != GIT_OK) {
            return std::unexpected(make_error_code(GitError::CheckoutFailed));
        }

        return {};
    }

    // --
    git_repository* repository = nullptr;
};

Repository::Repository(const std::filesystem::path& p)
    : impl_(std::make_unique<Internal>(p))
{
}

Repository::Repository(const std::string& url, const std::filesystem::path& p)
    : impl_(std::make_unique<Internal>(url, p))
{
}

Repository::~Repository() = default;

std::expected<void, std::error_code> Repository::update(const std::string& remote, const std::string& branch)
{
    return impl_->update(remote, branch);
}

}  // namespace scrap::repository::libgit
